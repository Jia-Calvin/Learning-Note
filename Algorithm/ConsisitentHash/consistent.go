package main

import (
	"errors"
	"hash/crc32"
	"sort"
	"strconv"
	"sync"
)

type uints []uint32

// Len returns the length of the uints array.
func (x uints) Len() int { return len(x) }

// Less returns true if element i is less than element j.
func (x uints) Less(i, j int) bool { return x[i] < x[j] }

// Swap exchanges elements i and j.
func (x uints) Swap(i, j int) { x[i], x[j] = x[j], x[i] }

// ErrEmptyCircle is the error returned when trying to get an element when nothing has been added to hash.
var ErrEmptyCircle = errors.New("empty circle")

// Consistent holds the information about the members of the consistent hash circle.
type Consistent struct {
	circle           map[uint32]string
	members          map[string]bool
	sortedHashKey    uints
	NumberOfReplicas int
	count            int64
	sync.RWMutex
}

// New creates a new Consistent object with a default setting of 20 replicas for each entry.
// To change the number of replicas, set NumberOfReplicas before adding entries.
func NewConsistentHash(replicas int) *Consistent {
	c := new(Consistent)
	// 每个节点的虚拟节点数量
	c.NumberOfReplicas = replicas
	c.circle = make(map[uint32]string)
	c.members = make(map[string]bool)
	return c
}

// 创建虚拟节点，加一个数字在string前面即可
func (c *Consistent) eltKey(elt string, idx int) string {
	return strconv.Itoa(idx) + elt
}

// Add inserts a string element in the consistent hash.
func (c *Consistent) Add(elt string) {
	c.Lock()
	defer c.Unlock()
	c.add(elt)
}

// need c.Lock() before calling
func (c *Consistent) add(elt string) {
	for i := 0; i < c.NumberOfReplicas; i++ {
		c.circle[c.hashKey(c.eltKey(elt, i))] = elt
	}
	c.members[elt] = true
	c.updateSortedHashes()
	c.count++
}

// Remove removes an element from the hash.
func (c *Consistent) Remove(elt string) {
	c.Lock()
	defer c.Unlock()
	c.remove(elt)
}

// need c.Lock() before calling
func (c *Consistent) remove(elt string) {
	for i := 0; i < c.NumberOfReplicas; i++ {
		delete(c.circle, c.hashKey(c.eltKey(elt, i)))
	}
	delete(c.members, elt)
	c.updateSortedHashes()
	c.count--
}

func (c *Consistent) Members() []string {
	c.RLock()
	defer c.RUnlock()
	var m []string
	for k := range c.members {
		m = append(m, k)
	}
	return m
}

// Get returns an element close to where name hashes to in the circle.
func (c *Consistent) Get(name string) (string, error) {
	c.RLock()
	defer c.RUnlock()
	if len(c.circle) == 0 {
		return "", ErrEmptyCircle
	}
	key := c.hashKey(name)
	i := c.search(key)
	return c.circle[c.sortedHashKey[i]], nil
}

func (c *Consistent) search(key uint32) (i int) {
	f := func(x int) bool {
		return c.sortedHashKey[x] > key
	}
	i = sort.Search(len(c.sortedHashKey), f)
	if i >= len(c.sortedHashKey) {
		i = 0
	}
	return
}

func (c *Consistent) hashKey(key string) uint32 {
	return crc32.ChecksumIEEE([]byte(key))
}

// 由于删除节点时从slice维护删除全部虚拟节点太耗时且难维护
// 干脆清空所有哈希值（空间没清），全部再往里添加。
func (c *Consistent) updateSortedHashes() {
	hashes := c.sortedHashKey[:0]
	//reallocate if we're holding on to too much (1/4th)
	if cap(c.sortedHashKey)/(c.NumberOfReplicas*4) > len(c.circle) {
		hashes = nil
	}
	for k := range c.circle {
		hashes = append(hashes, k)
	}
	sort.Sort(hashes)
	c.sortedHashKey = hashes
}
