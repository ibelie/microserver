// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

import (
	"fmt"
	"strings"
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
	observers map[ruid.RUID]map[string]map[ruid.RUID]bool
}

var HubInst = HubImpl{observers: make(map[ruid.RUID]map[string]map[ruid.RUID]bool)}

func HubService(server rpc.IServer, symbols map[string]uint64) (uint64, rpc.Service) {
	return SYMBOL_HUB, &HubInst
}

func (s *HubImpl) Procedure(i ruid.RUID, method uint64, param []byte) (result []byte, err error) {
	switch method {
	case SYMBOL_OBSERVE:
		if g, j := binary.Uvarint(param); j <= 0 {
			err = fmt.Errorf("[Hub] Observe parse gate session error %v: %v", j, param)
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
			err = fmt.Errorf("[Hub] Ignore parse gate session error %v: %v", j, param)
		} else {
			gate := string(param[j:])
			s.mutex.Lock()
			defer s.mutex.Unlock()
			if gates, ok := s.observers[i]; ok {
				if observers, ok := gates[gate]; ok {
					delete(observers, ruid.RUID(g))
					if len(observers) <= 0 {
						delete(gates, gate)
					}
				}
				if len(gates) <= 0 {
					delete(s.observers, i)
				}
			}
		}
	default:
		gates, ok := s.observers[i]
		if !ok || len(gates) <= 0 {
			err = fmt.Errorf("[Hub] Dispatch no gate found %v %v", i, gates)
			return
		}
		var errors []string
		for gate, observers := range gates {
			if len(observers) <= 0 {
				errors = append(errors, fmt.Sprintf("\n>>>> [Hub] Dispatch gate no observer %v %v",
					i, gate))
				continue
			}
			buffer := make([]byte, (len(observers)+1)*binary.MaxVarintLen64)
			buflen := binary.PutUvarint(buffer[:], uint64(len(observers)))
			for observer, _ := range observers {
				buflen += binary.PutUvarint(buffer[buflen:], uint64(observer))
			}
			data := append(buffer[:buflen], param...)
			if _, err = ServerInst.request(gate, i, SYMBOL_GATE, method, data); err != nil {
				errors = append(errors, fmt.Sprintf("\n>>>> gate: %v\n>>>> %v", gate, err))
			}
		}
		err = fmt.Errorf("[Hub] Dispatch errors %v %s(%v):%s", i, ServerInst.symdict[method], method, strings.Join(errors, ""))
	}
	return
}
