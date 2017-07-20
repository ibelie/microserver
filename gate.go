// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"log"
	"sync"

	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
)

const (
	GATE_NAME = "MICRO_GATE"
)

var (
	SYMBOL_GATE uint64
)

type Gate interface {
	Address() string
	Send([]byte) error
	Receive() ([]byte, error)
	Close() error
}

type GateImpl struct {
	mutex    sync.Mutex
	address  string
	services map[ruid.RUID]Gate
}

var GateInst = GateImpl{services: make(map[ruid.RUID]Gate)}

func GateService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_GATE, &GateInst
}

func (s *GateImpl) handler(conn Gate) {
	GateInst.mutex.Lock()

	GateInst.mutex.Unlock()
	for {
		if _, err := conn.Receive(); err != nil {
			log.Fatalf("[Gate@%v] Receive error %s:\n>>>>%v", s.address, conn.Address(), err)
		} else {

		}
	}
}

func (s *GateImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
