// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"log"
	"sync"

	"encoding/binary"
	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
)

const (
	GATE_NAME    = "MICRO_GATE"
	GATE_SESSION = "Session"
)

var (
	SYMBOL_GATE    uint64
	SYMBOL_SESSION uint64
)

type Gate interface {
	Address() string
	Send([]byte) error
	Receive() ([]byte, error)
	Close() error
}

type GateImpl struct {
	mutex sync.Mutex
	gates map[ruid.RUID]Gate
}

var GateInst = GateImpl{gates: make(map[ruid.RUID]Gate)}

func GateService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_GATE, &GateInst
}

func (s *GateImpl) handler(conn Gate) {
	session := ruid.New()
	comChan := make(chan []byte)
	var buffer [binary.MaxVarintLen64 + 1]byte
	buffer[0] = 0
	if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_CREATE,
		buffer[:1+binary.PutUvarint(buffer[1:], SYMBOL_SESSION)], nil); err != nil {
		log.Println("[Gate@%v] Create session error %v:\n>>>>%v", ServerInst.Address, session, err)
	} else if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_SYNCHRON, nil, comChan); err != nil {
		log.Println("[Gate@%v] Synchron session error %v:\n>>>>%v", ServerInst.Address, session, err)
	}
	var components [][]byte
	for component := range comChan {
		components = append(components, component)
	}
	if err := conn.Send(rpc.SerializeSession(session, ServerInst.symbols, components...)); err != nil {
		log.Println("[Gate@%v] Send session error %v:\n>>>>%v", ServerInst.Address, session, err)
	}

	GateInst.mutex.Lock()
	s.gates[session] = conn
	GateInst.mutex.Unlock()

	for {
		if _, err := conn.Receive(); err != nil {
			log.Println("[Gate@%v] Receive error %s:\n>>>>%v", ServerInst.Address, conn.Address(), err)
		} else {

		}
	}

	GateInst.mutex.Lock()
	delete(s.gates, session)
	GateInst.mutex.Unlock()
	if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_DESTROY, nil, nil); err != nil {
		log.Println("[Gate@%v] Destroy session error %v:\n>>>>%v", ServerInst.Address, session, err)
	}
}

func (s *GateImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	return
}
