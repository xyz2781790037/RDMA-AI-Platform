package main

import (
	schedulerlogger "rdma-ai-platform/scheduler/Logger"
	"rdma-ai-platform/scheduler/cmd"
)

func main() {
	if err := cmd.Run(); err != nil {
		schedulerlogger.Log.Fatal("scheduler exited", "err", err)
	}
}
