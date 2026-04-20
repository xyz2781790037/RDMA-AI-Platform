package cmd

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"time"

	schedulerlogger "rdma-ai-platform/scheduler/Logger"
	"rdma-ai-platform/scheduler/internal/api"
	grpcapi "rdma-ai-platform/scheduler/internal/grpcapi"
	"rdma-ai-platform/scheduler/internal/monitor"
	"rdma-ai-platform/scheduler/internal/registry"
	core "rdma-ai-platform/scheduler/internal/scheduler"
	"rdma-ai-platform/scheduler/internal/service"
	"rdma-ai-platform/scheduler/internal/storage"
)

type Config struct {
	Server    ServerConfig
	GRPC      GRPCConfig
	Scheduler SchedulerConfig
}

type ServerConfig struct {
	Addr              string
	ReadHeaderTimeout time.Duration
}

type GRPCConfig struct {
	Addr string
}

type SchedulerConfig struct {
	OfflineAfter    time.Duration
	MonitorInterval time.Duration
}

func Run() error {
	cfg, err := loadConfig(configPath())
	if err != nil {
		return err
	}

	if addr := os.Getenv("SCHEDULER_ADDR"); addr != "" {
		cfg.Server.Addr = addr
	}
	if addr := os.Getenv("SCHEDULER_GRPC_ADDR"); addr != "" {
		cfg.GRPC.Addr = addr
	}

	store := storage.NewMemoryStore()
	reg := registry.New(store, cfg.Scheduler.OfflineAfter)
	dispatcher := core.New(store, core.LeastLoadedStrategy{}, cfg.Scheduler.OfflineAfter)
	nodeService := service.NewNodeService(reg, dispatcher)
	taskService := service.NewTaskService(store, dispatcher)

	checker := monitor.NewChecker(dispatcher, cfg.Scheduler.MonitorInterval)
	checker.Start(context.Background())
	deliverer := monitor.NewAssignmentDeliverer(store, 300*time.Millisecond)
	deliverer.Start(context.Background())

	grpcListener, err := net.Listen("tcp", cfg.GRPC.Addr)
	if err != nil {
		return fmt.Errorf("listen grpc addr %q: %w", cfg.GRPC.Addr, err)
	}

	grpcServer := grpcapi.NewServer(nodeService, taskService, store)
	httpServer := &http.Server{
		Addr:              cfg.Server.Addr,
		Handler:           api.NewRouter(nodeService, taskService),
		ReadHeaderTimeout: cfg.Server.ReadHeaderTimeout,
	}

	schedulerlogger.Log.Info("scheduler http listening", "addr", cfg.Server.Addr)
	schedulerlogger.Log.Info("scheduler grpc listening", "addr", cfg.GRPC.Addr)

	errCh := make(chan error, 2)
	go func() {
		errCh <- grpcServer.Serve(grpcListener)
	}()
	go func() {
		errCh <- httpServer.ListenAndServe()
	}()

	return <-errCh
}

func configPath() string {
	if path := os.Getenv("SCHEDULER_CONFIG"); path != "" {
		return path
	}
	return "config/config.yaml"
}

func defaultConfig() Config {
	return Config{
		Server: ServerConfig{
			Addr:              ":8080",
			ReadHeaderTimeout: 5 * time.Second,
		},
		GRPC: GRPCConfig{
			Addr: ":9090",
		},
		Scheduler: SchedulerConfig{
			OfflineAfter:    30 * time.Second,
			MonitorInterval: 5 * time.Second,
		},
	}
}

func loadConfig(path string) (Config, error) {
	cfg := defaultConfig()

	file, err := os.Open(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return cfg, nil
		}
		return Config{}, err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	section := ""
	lineNumber := 0

	for scanner.Scan() {
		lineNumber++
		raw := scanner.Text()
		trimmed := strings.TrimSpace(raw)
		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}

		if !strings.HasPrefix(raw, " ") && !strings.HasPrefix(raw, "\t") {
			if !strings.HasSuffix(trimmed, ":") {
				return Config{}, fmt.Errorf("invalid config line %d: %s", lineNumber, raw)
			}
			section = strings.TrimSuffix(trimmed, ":")
			continue
		}

		key, value, ok := strings.Cut(strings.TrimSpace(raw), ":")
		if !ok {
			return Config{}, fmt.Errorf("invalid config line %d: %s", lineNumber, raw)
		}

		value = strings.TrimSpace(value)
		value = strings.Trim(value, `"'`)

		if err := applyConfigValue(&cfg, section, key, value); err != nil {
			return Config{}, fmt.Errorf("line %d: %w", lineNumber, err)
		}
	}

	if err := scanner.Err(); err != nil {
		return Config{}, err
	}

	return cfg, nil
}

func applyConfigValue(cfg *Config, section, key, value string) error {
	switch section {
	case "server":
		switch key {
		case "addr":
			cfg.Server.Addr = value
			return nil
		case "read_header_timeout":
			duration, err := time.ParseDuration(value)
			if err != nil {
				return fmt.Errorf("invalid server.read_header_timeout: %w", err)
			}
			cfg.Server.ReadHeaderTimeout = duration
			return nil
		default:
			return fmt.Errorf("unknown server key %q", key)
		}
	case "grpc":
		switch key {
		case "addr":
			cfg.GRPC.Addr = value
			return nil
		default:
			return fmt.Errorf("unknown grpc key %q", key)
		}
	case "scheduler":
		switch key {
		case "offline_after":
			duration, err := time.ParseDuration(value)
			if err != nil {
				return fmt.Errorf("invalid scheduler.offline_after: %w", err)
			}
			cfg.Scheduler.OfflineAfter = duration
			return nil
		case "monitor_interval":
			duration, err := time.ParseDuration(value)
			if err != nil {
				return fmt.Errorf("invalid scheduler.monitor_interval: %w", err)
			}
			cfg.Scheduler.MonitorInterval = duration
			return nil
		default:
			return fmt.Errorf("unknown scheduler key %q", key)
		}
	default:
		return fmt.Errorf("unknown config section %q", section)
	}
}
