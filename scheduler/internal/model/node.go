package model

import "time"

// NodeState 表示 worker 节点当前可用性的状态。
type NodeState string

const (
	// NodeReady 表示节点在线且还有可分配容量。
	NodeReady NodeState = "ready"
	// NodeBusy 表示节点在线但当前容量已满。
	NodeBusy NodeState = "busy"
	// NodeOffline 表示节点心跳超时或不可用。
	NodeOffline NodeState = "offline"
)

// Node 描述一个已注册到调度器中的 worker 节点。
type Node struct {
	// ID 是 worker 的唯一标识。
	ID string `json:"id"`
	// Address 是 worker 对外提供服务或回调的地址。
	Address string `json:"address"`
	// Capacity 是该 worker 允许并发执行的任务上限。
	Capacity int `json:"capacity"`
	// Labels 保存 worker 的标签或能力描述。
	Labels []string `json:"labels"`
	// RunningTasks 是当前在该 worker 上执行中的任务数量。
	RunningTasks int `json:"running_tasks"`
	// Status 是根据心跳和容量推导出的当前节点状态。
	Status NodeState `json:"status"`
	// LastHeartbeat 是最近一次收到该 worker 心跳的时间。
	LastHeartbeat time.Time `json:"last_heartbeat"`
}

// RegisterNodeRequest 是 worker 首次注册或更新静态信息时的请求体。
type RegisterNodeRequest struct {
	// ID 是要注册的 worker 标识。
	ID string `json:"id"`
	// Address 是 worker 当前监听的地址。
	Address string `json:"address"`
	// Capacity 是 worker 可承载的并发任务数。
	Capacity int `json:"capacity"`
	// Labels 用于描述 worker 的标签或能力集合。
	Labels []string `json:"labels"`
}

// HeartbeatRequest 是 worker 定期上报运行状态时的请求体。
type HeartbeatRequest struct {
	// ID 是发送心跳的 worker 标识。
	ID string `json:"id"`
	// RunningTasks 是 worker 当前正在执行的任务数。
	RunningTasks int `json:"running_tasks"`
	// Status 保留在心跳协议里，便于扩展或兼容外部状态上报。
	Status NodeState `json:"status"`
	// ActiveTaskIDs 保存 worker 当前仍在处理的任务 ID，供外部协议兼容使用。
	ActiveTaskIDs []string `json:"active_task_ids,omitempty"`
}

// DeriveNodeState 根据节点心跳时间和容量占用情况推导当前状态。
func DeriveNodeState(node *Node, now time.Time, offlineAfter time.Duration) NodeState {
	if node == nil {
		return NodeOffline
	}
	if !node.LastHeartbeat.IsZero() && now.Sub(node.LastHeartbeat) > offlineAfter {
		return NodeOffline
	}
	if node.Capacity > 0 && node.RunningTasks >= node.Capacity {
		return NodeBusy
	}
	return NodeReady
}

// CloneNode 返回节点的值拷贝，并复制 Labels 切片，避免共享底层数组。
func CloneNode(node *Node) Node {
	if node == nil {
		return Node{}
	}

	clone := *node
	clone.Labels = append([]string(nil), node.Labels...)
	return clone
}
