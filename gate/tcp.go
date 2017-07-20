// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package gate

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

type TcpGate struct {
	net.Conn
	data   []byte
	buffer [BUFFER_SIZE]byte
}

func (g *TcpGate) Address() string {
	return g.RemoteAddr().String()
}

func (g *TcpGate) Send(data []byte) (err error) {
	var lenBuf [binary.MaxVarintLen64]byte
	_, err = g.Conn.Write(append(lenBuf[:binary.PutUvarint(lenBuf[:], uint64(len(data)))], data...))
	return
}

func (g *TcpGate) Receive() (data []byte, err error) {
	var n int
	var length uint64
	var hasLength bool
	for {
		if n, err = g.Conn.Read(g.buffer[:]); err != nil {
			if err == io.EOF || isClosedConnError(err) {
				err = fmt.Errorf("[TcpGate@%v] Connection lost:\n>>>>%v", g.Address(), err)
			} else if e, ok := err.(net.Error); ok && e.Timeout() {
				err = fmt.Errorf("[TcpGate@%v] Connection timeout:\n>>>>%v", g.Address(), e)
			} else {
				err = fmt.Errorf("[TcpGate@%v] Connection error:\n>>>>%v", g.Address(), err)
			}
			return
		} else {
			g.data = append(g.data, g.buffer[:n]...)
		}
		if !hasLength {
			length = 0
			var k uint
			for i, b := range g.data {
				if b < 0x80 {
					if i > 9 || i == 9 && b > 1 {
						err = fmt.Errorf("[TcpGate@%v] Request protocol error: %v %v",
							g.Address(), g.data[:i], length)
						return
					}
					length |= uint64(b) << k
					g.data = g.data[i+1:]
					hasLength = true
				}
				length |= uint64(b&0x7f) << k
				k += 7
			}
		}
		if hasLength && uint64(len(g.data)) >= length {
			data = g.data[:length]
			g.data = g.data[length:]
			return
		}
	}
}

func Tcp(address string, handler func(microserver.Gate)) {
	if lis, err := net.Listen("tcp", address); err != nil {
		log.Fatalf("[TCP@%v] Cannot listen:\n>>>>%v", address, err)
	} else {
		log.Printf("[TCP@%v] Waiting for clients...", address)
		defer lis.Close()
		for {
			if conn, err := lis.Accept(); err != nil {
				log.Printf("[TCP@%v] Accept error:\n>>>>%v", address, err)
			} else {
				go func() {
					defer conn.Close()
					handler(&TcpGate{Conn: conn})
				}()
			}
		}
	}
}
