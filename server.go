// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"reflect"
	"runtime"
	"strings"
	"sync"
	"time"

	"encoding/json"
	"github.com/coreos/etcd/client"
	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
	"golang.org/x/net/context"
)

const BUFFER_SIZE = 4096

// Copy from golang.org\x\net\http2\server.go
func errno(v error) uintptr {
	if rv := reflect.ValueOf(v); rv.Kind() == reflect.Uintptr {
		return uintptr(rv.Uint())
	}
	return 0
}

// Copy from golang.org\x\net\http2\server.go
func isClosedConnError(err error) bool {
	if err == nil {
		return false
	}

	str := err.Error()
	if strings.Contains(str, "use of closed network connection") {
		return true
	}

	if runtime.GOOS == "windows" {
		if oe, ok := err.(*net.OpError); ok && oe.Op == "read" {
			if se, ok := oe.Err.(*os.SyscallError); ok && se.Syscall == "wsarecv" {
				const WSAECONNABORTED = 10053
				const WSAECONNRESET = 10054
				if n := errno(se.Err); n == WSAECONNRESET || n == WSAECONNABORTED {
					return true
				}
			}
		}
	}
	return false
}

type Node struct {
	Address  string
	Services []uint64
}

type Server struct {
	Node
	mutex   sync.Mutex
	nodes   map[string]*Node
	symbols map[string]uint64
	routes  map[uint64]map[uint64]bool
	remote  map[uint64]*ruid.Ring
	local   map[uint64]rpc.Service
}

var ServerInst rpc.IServer

func NewServer(address string, symbols map[string]uint64,
	routes map[uint64]map[uint64]bool, rs ...rpc.Register) *Server {
	server := &Server{
		Node:    Node{Address: address},
		symbols: symbols,
		routes:  routes,
		nodes:   make(map[string]*Node),
		remote:  make(map[uint64]*ruid.Ring),
		local:   make(map[uint64]rpc.Service),
	}
	SYMBOL_GATE = addSymbol(symbols, GATE_NAME)
	SYMBOL_HUB = addSymbol(symbols, HUB_NAME)
	SYMBOL_NOTIFY = addSymbol(symbols, HUB_NOTIFY)
	ServerInst = server

	for _, r := range rs {
		i, c := r(server, symbols)
		server.remote[i] = ruid.NewRing(address)
		server.Services = append(server.Services, i)
		server.local[i] = c
	}
	return server
}

func (s *Server) Notify(i ruid.RUID, k ruid.RUID, p []byte) (err error) {
	_, err = s.Procedure(i, k, SYMBOL_HUB, SYMBOL_NOTIFY, p)
	return
}

func (s *Server) Distribute(i ruid.RUID, k ruid.RUID, t uint64, m uint64, p []byte, r chan<- []byte) (err error) {
	return
}

func (s *Server) Procedure(i ruid.RUID, k ruid.RUID, c uint64, m uint64, p []byte) (r []byte, err error) {
	if k == 0 {
		k = i
	}
	return
}

func (s *Server) Gate(address string) {

}

func (s *Server) handler(conn net.Conn) {
	var id ruid.RUID
	var service, method, length, step uint64
	var data []byte
	buffer := make([]byte, BUFFER_SIZE)
	defer conn.Close()
	for {
		if n, err := conn.Read(buffer); err != nil {
			if err == io.EOF || isClosedConnError(err) {
				log.Printf("[MicroServer@%v] Connection lost:\n>>>>%v", s.Address, err)
			} else if e, ok := err.(net.Error); ok && e.Timeout() {
				log.Printf("[MicroServer@%v] Connection timeout:\n>>>>%v", s.Address, e)
			} else {
				log.Printf("[MicroServer@%v] Connection error:\n>>>>%v", s.Address, err)
			}
			return
		} else {
			data = append(data, buffer[:n]...)
		}
		for step < 4 {
			var x, k uint64
			for i, b := range data {
				if b < 0x80 {
					if i > 9 || i == 9 && b > 1 {
						log.Printf("[MicroServer@%v] Protocol error: %v %v %v %v %v %v",
							s.Address, data[:i], id, service, method, length, step)
						return // overflow
					}
					x |= uint64(b) << k
					data = data[i+1:]
				}
				x |= uint64(b&0x7f) << k
				k += 7
			}
			switch step {
			case 0:
				id = ruid.RUID(x)
			case 1:
				service = x
			case 2:
				method = x
			case 3:
				length = x
			}
			step++
		}
		if step == 4 && uint64(len(data)) >= length {
			param := data[:length]
			data = data[length:]
			if services, ok := s.local[service]; !ok {
				log.Printf("[MicroServer@%v] Service (%d) not exists", s.Address, service)
			} else if result, err := services.Procedure(id, method, param); err != nil {
				log.Printf("[MicroServer@%v] Procedure error:\n>>>>%v", s.Address, err)
			} else if _, err := conn.Write(result); err != nil {
				log.Printf("[MicroServer@%v] Response error:\n>>>>%v", s.Address, err)
			}
			step = 0
		}
	}
}

func (s *Server) Serve() {
	if lis, err := net.Listen("tcp", s.Address); err != nil {
		log.Fatalf("[MicroServer@%v] Cannot listen:\n>>>>%v", s.Address, err)
	} else {
		defer lis.Close()
		log.Printf("[MicroServer@%v] Waiting for clients...", s.Address)
		for {
			if conn, err := lis.Accept(); err != nil {
				log.Printf("[MicroServer@%v] Accept error:\n>>>>%v", s.Address, err)
			} else {
				go s.handler(conn)
			}
		}
	}
}

func (s *Server) Register(nodes ...*Node) {
	for _, node := range nodes {
		for _, service := range node.Services {
			if ring, ok := s.remote[service]; ok {
				ring.Append(node.Address)
			} else {
				s.remote[service] = ruid.NewRing(node.Address)
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
		go s.keep(namespace, api)
		go s.watch(namespace, api)
	} else {
		log.Fatalf("[MicroServer] Cannot connect to etcd:\n>>>>%v", err)
	}
}

func (s *Server) Add(key string, node *Node) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	for _, service := range node.Services {
		if ring, ok := s.remote[service]; ok {
			ring.Append(node.Address)
		} else {
			s.remote[service] = ruid.NewRing(node.Address)
		}
	}
	s.nodes[key] = node
}

func (s *Server) Remove(key string) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	if node, ok := s.nodes[key]; ok {
		for _, service := range node.Services {
			s.remote[service].Remove(node.Address)
		}
	}
	delete(s.nodes, key)
}

func (s *Server) keep(namespace string, api client.KeysAPI) {
	key := namespace + s.Address
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

func addSymbol(symbols map[string]uint64, name string) (symbol uint64) {
	if _, exist := symbols[name]; exist {
		log.Fatalf("[MicroServer] Symbol '%s' already exists", name)
	}
	for _, s := range symbols {
		if symbol <= s {
			symbol = s + 1
		}
	}
	symbols[name] = symbol
	return
}
