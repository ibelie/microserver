// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"sync"

	"encoding/binary"
	"github.com/ibelie/rpc"
	"github.com/ibelie/ruid"
)

const (
	HUB_NAME    = "MICRO_HUB"
	HUB_NOTIFY  = "NOTIFY"
	HUB_OBSERVE = "OBSERVE"
	HUB_IGNORE  = "IGNORE"
)

var (
	SYMBOL_HUB     uint64
	SYMBOL_NOTIFY  uint64
	SYMBOL_OBSERVE uint64
	SYMBOL_IGNORE  uint64
)

type HubImpl struct {
	mutex     sync.Mutex
	conns     map[string]*sync.Pool
	observers map[ruid.RUID]map[string]map[ruid.RUID]bool
}

var HubInst = HubImpl{conns: make(map[string]*sync.Pool), services: make(map[ruid.RUID]string)}

func HubService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_HUB, &HubInst
}

func (s *HubImpl) dispatch(i ruid.RUID, m uint64, p []byte) (err error) {
	gates, ok := s.observers[i]
	if !ok || len(gates) <= 0 {
		return
	}
	for node, observers := range gates {
		if len(observers) <= 0 {
			continue
		}
		var buffer [(len(observers) + 2) * binary.MaxVarintLen64]byte
		buflen := binary.PutUvarint(buffer[:], len(observers))
		for observer, ok := range observers {
			if ok {
				buflen += binary.PutUvarint(buffer[buflen:], uint64(observer))
			}
		}
		buflen += binary.PutUvarint(buffer[buflen:], m)
		data := append(buffer[:buflen], p...)
		if node == ServerInst.Address {
			if gate, ok := s.local[SYMBOL_GATE]; !ok {
				err = fmt.Errorf("[Hub@%v] Dispatch no local gate found: %v %v", ServerInst.Address, s.Node, s.local)
			} else {
				_, err = gate.Procedure(0, 0, data)
			}
			continue
		}

		if _, ok := s.conns[node]; !ok {
			s.mutex.Lock()
			s.conns[node] = &sync.Pool{New: func() interface{} {
				if conn, err := net.DialTimeout("tcp", node, CONN_DEADLINE*time.Second); err != nil {
					log.Printf("[Hub@%v] Connection failed: %s\n>>>>%v", ServerInst.Address, node, err)
					return nil
				} else {
					return conn
				}
			}}
			s.mutex.Unlock()
		}

		for j := 0; j < 3; j++ {
			if o := s.conns[node].Get(); o == nil {
				continue
			} else if conn, ok := o.(net.Conn); !ok {
				log.Printf("[Hub@%v] Connection pool type error: %v", ServerInst.Address, o)
				continue
			} else if err = conn.SetWriteDeadline(time.Now().Add(time.Second * WRITE_DEADLINE)); err != nil {
			} else if _, err = conn.Write(data); err != nil {
			} else {
				s.conns[node].Put(conn)
				break
			}

			if err != nil {
				if err == io.EOF || isClosedConnError(err) {
				} else if e, ok := err.(net.Error); ok && e.Timeout() {
				} else {
					log.Printf("[Hub@%v] Dispatch retry:\n>>>>%v", ServerInst.Address, err)
				}
			}
		}
	}

	return
}

func (s *HubImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	switch method {
	case SYMBOL_OBSERVE:
		if g, j := binary.Uvarint(param); j <= 0 {
			err = fmt.Errorf("[Hub@%v] Observe parse gate session error %v: %v", ServerInst.Address, j, param)
		} else {
			gate := string(param[j:])
			s.mutex.Lock()
			defer s.mutex.Unlock()
			if _, ok := s.observers[i]; !ok {
				s.observers[i] = make(map[string]map[ruid.RUID]bool)
			}
			if _, ok := s.observers[i][gate]; !ok {
				s.observers[i][gate] = make(map[ruid.RUID]bool)
			}
			s.observers[i][gate][ruid.RUID(g)] = true
		}
	case SYMBOL_IGNORE:
		if g, j := binary.Uvarint(param); j <= 0 {
			err = fmt.Errorf("[Hub@%v] Ignore parse gate session error %v: %v", ServerInst.Address, j, param)
		} else {
			gate := string(param[j:])
			s.mutex.Lock()
			defer s.mutex.Unlock()
			if gates, ok := s.observers[i]; ok {
				if observers, ok := gates[gate]; ok {
					delete(observers, ruid.RUID(g))
				}
			}
		}
	default:
		s.dispatch(i, method, param)
	}
	return
}
