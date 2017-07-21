// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package network

import (
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"reflect"
	"runtime"
	"strings"

	"encoding/binary"
	"github.com/ibelie/microserver"
)

const (
	CONN_DEADLINE  = 5
	READ_DEADLINE  = 30
	WRITE_DEADLINE = 5
	BUFFER_SIZE    = 4096
)

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

func (s *Server) Serve() {
	if lis, err := net.Listen("tcp", s.Address()); err != nil {
		log.Fatalf("[MicroServer@%v] Cannot listen:\n>>>> %v", s.Address(), err)
	} else {
		defer lis.Close()
		log.Printf("[MicroServer@%v] Waiting for clients...", s.Address())
		for {
			if conn, err := lis.Accept(); err != nil {
				log.Printf("[MicroServer@%v] Accept error:\n>>>> %v", s.Address(), err)
			} else {
				go s.response(conn)
			}
		}
	}
}

func (s *Server) request(node string, i ruid.RUID, c uint64, m uint64, p []byte) (r []byte, err error) {
	if node == s.Address() {
		if service, ok := s.local[c]; !ok {
			err = fmt.Errorf("[Request] No local service found: %s(%v) %v %v", s.symdict[c], c, s.Node, s.local)
		} else {
			r, err = service.Procedure(i, m, p)
		}
		return
	}

	if _, ok := s.conns[node]; !ok {
		s.mutex.Lock()
		s.conns[node] = &sync.Pool{New: func() interface{} {
			if conn, err := net.DialTimeout("tcp", node, CONN_DEADLINE*time.Second); err != nil {
				log.Printf("[MicroServer@%v] Connection failed: %s\n>>>> %v", s.Address(), node, err)
				return nil
			} else {
				return conn
			}
		}}
		s.mutex.Unlock()
	}

	var n int
	var length uint64
	var hasLength bool
	var header [binary.MaxVarintLen64 * 4]byte
	var buffer [BUFFER_SIZE]byte
	buflen := binary.PutUvarint(header[:], uint64(i))
	buflen += binary.PutUvarint(header[buflen:], c)
	buflen += binary.PutUvarint(header[buflen:], m)
	buflen += binary.PutUvarint(header[buflen:], uint64(len(p)))
	param := append(header[:buflen], p...)

	for j := 0; j < 3; j++ {
		if o := s.conns[node].Get(); o == nil {
			continue
		} else if conn, ok := o.(net.Conn); !ok {
			log.Printf("[MicroServer@%v] Connection pool type error: %v", s.Address(), o)
			continue
		} else if err = conn.SetWriteDeadline(time.Now().Add(time.Second * WRITE_DEADLINE)); err != nil {
		} else if _, err = conn.Write(param); err != nil {
		} else {
			var result []byte
			for {
				if n, err = conn.Read(buffer[:]); err != nil {
					if err == io.EOF || isClosedConnError(err) {
						err = fmt.Errorf("[Request] Connection lost:\n>>>> %v", err)
					} else if e, ok := err.(net.Error); ok && e.Timeout() {
						err = fmt.Errorf("[Request] Connection timeout:\n>>>> %v", e)
					} else {
						err = fmt.Errorf("[Request] Connection error:\n>>>> %v", err)
					}
					return
				} else {
					result = append(result, buffer[:n]...)
				}
				if !hasLength {
					length = 0
					var k uint
					for l, b := range result {
						if b < 0x80 {
							if l > 9 || l == 9 && b > 1 {
								err = fmt.Errorf("[Request] Response protocol error: %v %v",
									result[:l], length)
								return
							}
							length |= uint64(b) << k
							result = result[l+1:]
							hasLength = true
						}
						length |= uint64(b&0x7f) << k
						k += 7
					}
				}
				if hasLength && uint64(len(result)) >= length {
					if r = result[:length]; uint64(len(result)) > length {
						log.Printf("[MicroServer@%v] Ignore response data: %v", s.Address(), result)
					}
					break
				}
			}
			s.conns[node].Put(conn)
			break
		}

		if err != nil {
			if err == io.EOF || isClosedConnError(err) {
			} else if e, ok := err.(net.Error); ok && e.Timeout() {
			} else {
				log.Printf("[MicroServer@%v] Request retry:\n>>>> %v", s.Address(), err)
			}
		}
	}

	return
}

func (s *Server) response(conn net.Conn) {
	var id ruid.RUID
	var service, method, length, step uint64
	var data []byte
	var lenBuf [binary.MaxVarintLen64]byte
	var buffer [BUFFER_SIZE]byte
	defer conn.Close()
	for {
		conn.SetReadDeadline(time.Now().Add(time.Second * READ_DEADLINE))
		if n, err := conn.Read(buffer[:]); err != nil {
			if err == io.EOF || isClosedConnError(err) {
				log.Printf("[MicroServer@%v] Connection lost:\n>>>> %v", s.Address(), err)
			} else if e, ok := err.(net.Error); ok && e.Timeout() {
				log.Printf("[MicroServer@%v] Connection timeout:\n>>>> %v", s.Address(), e)
			} else {
				log.Printf("[MicroServer@%v] Connection error:\n>>>> %v", s.Address(), err)
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
						log.Printf("[MicroServer@%v] Request protocol error: %v %v %s(%v) %s(%v) %v %v",
							s.Address(), data[:i], id, s.symdict[service], service, s.symdict[method], method, length, step)
						return // overflow
					}
					x |= uint64(b) << k
					data = data[i+1:]
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
				x |= uint64(b&0x7f) << k
				k += 7
			}
		}
		if step == 4 && uint64(len(data)) >= length {
			param := data[:length]
			data = data[length:]
			if services, ok := s.local[service]; !ok {
				log.Printf("[MicroServer@%v] Service %s(%v) not exists", s.Address(), s.symdict[service], service)
			} else if result, err := services.Procedure(id, method, param); err != nil {
				log.Printf("[MicroServer@%v] Procedure error:\n>>>> %v", s.Address(), err)
			} else if _, err := conn.Write(append(lenBuf[:binary.PutUvarint(lenBuf[:], uint64(len(result)))], result...)); err != nil {
				log.Printf("[MicroServer@%v] Response error:\n>>>> %v", s.Address(), err)
			}
			step = 0
		}
	}
}
