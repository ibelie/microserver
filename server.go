// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
	"log"
	"sync"
	"time"

	"encoding/json"
	"github.com/coreos/etcd/client"
	"github.com/ibelie/ruid"
	"golang.org/x/net/context"
)

type Node struct {
	Address  string
	Services []uint64
}

type Service interface {
	Procedure(ruid.RUID, uint64, []byte) ([]byte, error)
}

type IServer interface {
	Distribute(ruid.RUID, ruid.RUID, uint64, uint64, []byte, chan<- []byte) error
	Procedure(ruid.RUID, ruid.RUID, uint64, uint64, []byte) ([]byte, error)
}

type Register func(IServer, map[string]uint64) Service

type Server struct {
	Node
	mutex   sync.Mutex
	nodes   map[string]*Node
	symbols map[string]uint64
	routes  map[uint64]map[uint64]bool
	remote  map[uint64]HashRing
	local   map[uint64]Service
}

func NewServer(address string, symbols map[string]uint64, routes map[uint64]map[uint64]bool, rs ...Register) {
	server := &Server{
		Address: address,
		symbols: symbols,
		routes:  routes,
		nodes:   make(map[string]*Node),
		remote:  make(map[uint64]HashRing),
		local:   make(map[uint64]Service),
	}
	for _, r := range rs {
		i, c := r(s, symbols)
		s.remote[i] = HashRing([]string{address})
		s.Services = append(s.Services, i)
		s.local[i] = c
	}
}

func (s *Server) Distribute(i ruid.RUID, k ruid.RUID, t uint64, m uint64, p []byte, r chan<- []byte) (err error) {

}

func (s *Server) Procedure(i ruid.RUID, k ruid.RUID, c uint64, m uint64, p []byte) (r []byte, err error) {

}

func (s *Server) Serve() {

}

func (s *Server) Register(nodes ...*Node) {
	for _, node := range nodes {
		for _, service := range node.Services {
			if hashring, ok := s.remote[service]; ok {
				s.remote[service] = hashring.Append(node.Address)
			} else {
				s.remote[service] = HashRing([]string{node.Address})
			}
		}
		s.nodes[node.Address] = node
	}
}

func (s *Server) Discover(ip string, port int, namespace string) {
	cfg := client.Config{
		Endpoints:               []string{fmt.Sprintf("http://%s:%d", ip, port)},
		Transport:               client.DefaultTransport,
		HeaderTimeoutPerRequest: time.Second,
	}

	if c, err := client.New(cfg); err == nil {
		api := client.NewKeysAPI(c)
		go server.keep(namespace, api)
		go server.watch(namespace, api)
	} else {
		log.Fatalf("[MicroServer] Cannot connect to etcd:\n>>>>%v", err)
	}
}

func (s *Server) Add(key string, node *Node) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	for _, service := range node.Services {
		if hashring, ok := s.remote[service]; ok {
			s.remote[service] = hashring.Append(node.Address)
		} else {
			s.remote[service] = HashRing([]string{node.Address})
		}
	}
	s.nodes[key] = node
}

func (s *Server) Remove(key string) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	if node, ok := s.nodes[key]; ok {
		for _, service := range node.Services {
			s.remote[service] = s.remote[service].Remove(node.Address)
		}
	}
	delete(s.nodes, key)
}

func (s *Server) keep(namespace string, api client.KeysAPI) {
	key := namespace + address
	value, _ := json.Marshal(&s.Node)
	for {
		if _, err := api.Set(context.Background(), key, string(value), &client.SetOptions{TTL: time.Second * 10}); err != nil {
			log.Printf("[MicroServer] Update server info:\n>>>> %v", err)
		}
		time.Sleep(time.Second * 3)
	}
}

func (s *Server) watch(namespace string, api client.KeysAPI) {
	watcher := api.Watcher(namespace, &client.WatcherOptions{Recursive: true})
	for {
		if res, err := watcher.Next(context.Background()); err == nil {
			if res.Action == "expire" || res.Action == "delete" {
				s.Remove(res.Node.Key)
			} else if res.Action == "set" || res.Action == "update" {
				node := new(Node)
				if err := json.Unmarshal([]byte(res.Node.Value), node); err != nil {
					log.Printf("[MicroServer] Parse node value:\n>>>> %v", err)
					break
				}
				s.Add(res.Node.Key, node)
			}
		} else {
			log.Printf("[MicroServer] Watch servers:\n>>>> %v", err)
		}
	}
}
