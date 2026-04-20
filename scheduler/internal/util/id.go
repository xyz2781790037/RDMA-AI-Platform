package util

import "fmt"

// FormatTaskID 将递增计数器格式化为稳定的任务 ID。
func FormatTaskID(counter uint64) string {
	return fmt.Sprintf("task-%06d", counter)
}
