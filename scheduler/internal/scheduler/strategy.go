package scheduler

import "rdma-ai-platform/scheduler/internal/model"

// Strategy 定义多个 worker 候选之间的比较规则。
type Strategy interface {
	// Compare 比较两个候选节点的优先级。
	// 返回值小于 0 表示 left 更优，大于 0 表示 right 更优，等于 0 表示两者等价。
	Compare(left, right *model.Node) int
}

// LeastLoadedStrategy 优先选择负载比例更低、剩余容量更多的 worker。
type LeastLoadedStrategy struct{}
// 把 Node 原始数据整理成用于比较的统一格式
type nodeLoadSnapshot struct {
	runningTasks int
	capacity     int
	available    int
}

// Compare 比较两个节点的负载，返回哪个节点更适合接收新任务。
func (LeastLoadedStrategy) Compare(left, right *model.Node) int {
	switch {
	case left == nil && right == nil:
		return 0
	case left == nil:
		return 1
	case right == nil:
		return -1
	}

	leftLoad := snapshotNodeLoad(left)
	rightLoad := snapshotNodeLoad(right)

	if diff := compareLoadRatio(leftLoad, rightLoad); diff != 0 {
		return diff
	}
	if diff := compareDescending(leftLoad.available, rightLoad.available); diff != 0 {
		return diff
	}
	if diff := compareAscending(leftLoad.runningTasks, rightLoad.runningTasks); diff != 0 {
		return diff
	}
	return 0
}

func snapshotNodeLoad(node *model.Node) nodeLoadSnapshot {
	if node == nil {
		return nodeLoadSnapshot{capacity: 1}
	}

	runningTasks := node.RunningTasks
	if runningTasks < 0 {
		runningTasks = 0
	}

	capacity := node.Capacity
	if capacity <= 0 {
		capacity = 1
	}

	available := capacity - runningTasks
	if available < 0 {
		available = 0
	}

	return nodeLoadSnapshot{
		runningTasks: runningTasks,
		capacity:     capacity,
		available:    available,
	}
}

func compareLoadRatio(left, right nodeLoadSnapshot) int {
	leftValue := int64(left.runningTasks) * int64(right.capacity)
	rightValue := int64(right.runningTasks) * int64(left.capacity)

	switch {
	case leftValue < rightValue:
		return -1
	case leftValue > rightValue:
		return 1
	default:
		return 0
	}
}

func compareAscending(left, right int) int {
	switch {
	case left < right:
		return -1
	case left > right:
		return 1
	default:
		return 0
	}
}

func compareDescending(left, right int) int {
	return compareAscending(right, left)
}
