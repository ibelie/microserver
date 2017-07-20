// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
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

func (s *GateImpl) register(i ruid.RUID, k ruid.RUID, m uint64, t ruid.RUID) (err error) {
	var buffer [binary.MaxVarintLen64]byte
	_, err = ServerInst.Procedure(i, k, SYMBOL_HUB, m,
		append(buffer[:binary.PutUvarint(buffer[:], uint64(t))], []byte(ServerInst.Address)...))
	return
}

func (s *GateImpl) handler(gate Gate) {
	session := ruid.New()
	comChan := make(chan []byte)
	var buffer [binary.MaxVarintLen64 + 1]byte
	buffer[0] = 0
	if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_CREATE,
		buffer[:1+binary.PutUvarint(buffer[1:], SYMBOL_SESSION)], nil); err != nil {
		log.Printf("[Gate@%v] Create session error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), session, err)
	} else if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_SYNCHRON, nil, comChan); err != nil {
		log.Printf("[Gate@%v] Synchron session error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), session, err)
	}
	var components [][]byte
	for component := range comChan {
		components = append(components, component)
	}
	if err := s.register(session, 0, SYMBOL_OBSERVE, session); err != nil {
		log.Printf("[Gate@%v] Observe session error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), session, err)
	} else if err := gate.Send(rpc.SerializeSession(session, ServerInst.symbols, components...)); err != nil {
		log.Printf("[Gate@%v] Send session error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), session, err)
	}

	GateInst.mutex.Lock()
	s.gates[session] = gate
	GateInst.mutex.Unlock()

	for {
		if p, err := gate.Receive(); err != nil {
			log.Printf("[Gate@%v] Receive error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), session, err)
			break
		} else if i, o1 := binary.Uvarint(p); o1 <= 0 {
			log.Printf("[Gate@%v] Parse RUID error %v %v: %v %v", ServerInst.Address, gate.Address(), session, o1, p)
			break
		} else if k, o2 := binary.Uvarint(p[o1:]); o2 <= 0 {
			log.Printf("[Gate@%v] Parse Key error %v %v: %v %v", ServerInst.Address, gate.Address(), session, o2, p[o1:])
			break
		} else if t, o3 := binary.Uvarint(p[o1+o2:]); o3 <= 0 {
			log.Printf("[Gate@%v] Parse Type error %v %v: %v %v", ServerInst.Address, gate.Address(), session, o3, p[o1+o2:])
			break
		} else if m, o4 := binary.Uvarint(p[o1+o2+o3:]); o4 <= 0 {
			log.Printf("[Gate@%v] Parse method error %v %v: %v %v", ServerInst.Address, gate.Address(), session, o4, p[o1+o2+o3:])
			break
		} else {
			p = p[o1+o2+o3+o4:]
			switch m {
			case SYMBOL_OBSERVE:
				if err := s.register(i, k, SYMBOL_OBSERVE, session); err != nil {
					log.Printf("[Gate@%v] Observe %s(%v:%v) error %v %v:\n>>>>%v", ServerInst.Address, ServerInst.symdict[t], i, k, gate.Address(), session, err)
				}
			case SYMBOL_IGNORE:
				if err := s.register(i, k, SYMBOL_IGNORE, session); err != nil {
					log.Printf("[Gate@%v] Ignore %s(%v:%v) error %v %v:\n>>>>%v", ServerInst.Address, ServerInst.symdict[t], i, k, gate.Address(), session, err)
				}
			default:
				if err := ServerInst.Distribute(i, k, t, m, p, nil); err != nil {
					log.Printf("[Gate@%v] Distribute %s(%v) to %s(%v:%v) error %v %v:\n>>>>%v", ServerInst.Address, ServerInst.symdict[m], m, ServerInst.symdict[t], i, k, gate.Address(), session, err)
				}
			}
		}
	}

	GateInst.mutex.Lock()
	delete(s.gates, session)
	GateInst.mutex.Unlock()
	if err := s.register(session, 0, SYMBOL_IGNORE, session); err != nil {
		log.Printf("[Gate@%v] Ignore session error %v:\n>>>>%v", ServerInst.Address, session, err)
	} else if err := ServerInst.Distribute(session, 0, SYMBOL_SESSION, rpc.SYMBOL_DESTROY, nil, nil); err != nil {
		log.Printf("[Gate@%v] Destroy session error %v:\n>>>>%v", ServerInst.Address, session, err)
	}
}

func (s *GateImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	var j, k int
	var m, n uint64
	if n, j = binary.Uvarint(param); j <= 0 {
		err = fmt.Errorf("[Gate@%v] Dispatch parse count error %v: %v", ServerInst.Address, j, param)
		return
	}
	var observers []ruid.RUID
	for l := uint64(0); l < n; l++ {
		if m, k := binary.Uvarint(param[j:]); k <= 0 {
			err = fmt.Errorf("[Gate@%v] Dispatch parse observer error %v: %v", ServerInst.Address, k, param[j:])
			return
		} else {
			j += k
			observers = append(observers, ruid.RUID(m))
		}
	}
	param = param[j:]
	for _, observer := range observers {
		if gate, ok := s.gates[observer]; !ok {
			log.Printf("[Gate@%v] Dispatch gate not found %v", ServerInst.Address, observer)
		} else if err = gate.Send(param); err != nil {
			log.Printf("[Gate@%v] Dispatch error %v %v:\n>>>>%v", ServerInst.Address, gate.Address(), err)
			break
		}
	}
	return
}
