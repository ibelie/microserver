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
	"time"

	"github.com/ibelie/rpc"
)

const (
	CONN_DEADLINE  = 10
	READ_DEADLINE  = 300
	WRITE_DEADLINE = 30
	BUFFER_SIZE    = 4096
)

type TcpConn struct {
	net.Conn
	data   []byte
	buffer [BUFFER_SIZE]byte
}

func (c *TcpConn) Address() string {
	return c.RemoteAddr().String()
}

func (c *TcpConn) Send(data []byte) (err error) {
	if err = c.Conn.SetWriteDeadline(time.Now().Add(time.Second * WRITE_DEADLINE)); err == nil {
		_, err = c.Conn.Write(Pack(data))
	}
	return
}

func (c *TcpConn) Receive() (data []byte, err error) {
	for {
		if err = c.Conn.SetReadDeadline(time.Now().Add(time.Second * READ_DEADLINE)); err != nil {
			return
		} else if n, e := c.Conn.Read(c.buffer[:]); e != nil {
			if e == io.EOF || isClosedConnError(e) {
				err = fmt.Errorf("[Tcp] Connection %q lost:\n>>>> %v", c.Address(), e)
			} else if ee, ok := e.(net.Error); ok && ee.Timeout() {
				err = fmt.Errorf("[Tcp] Connection %q timeout:\n>>>> %v", c.Address(), ee)
			} else {
				err = fmt.Errorf("[Tcp] Connection %q error:\n>>>> %v", c.Address(), e)
			}
			return
		} else {
			c.data = Extend(c.data, c.buffer[:n])
		}
		if data, err = Unpack(c.data); err != nil {
			err = fmt.Errorf("[Tcp] Connection %q error:\n>>>> %v", c.Address(), err)
		} else if len(data) > 0 {
			return
		}
	}
}

type TcpNet int

var Tcp TcpNet = 0

func (_ TcpNet) Serve(address string, handler func(rpc.Connection)) {
	if lis, err := net.Listen("tcp", address); err != nil {
		log.Fatalf("[Tcp@%v] Cannot listen:\n>>>> %v", address, err)
	} else {
		defer lis.Close()
		log.Printf("[Tcp@%v] Waiting for clients...", address)
		for {
			if conn, err := lis.Accept(); err != nil {
				log.Printf("[Tcp@%v] Accept error:\n>>>> %v", address, err)
			} else {
				go func() {
					defer conn.Close()
					handler(&TcpConn{Conn: conn})
				}()
			}
		}
	}
}

func (_ TcpNet) Connect(address string) rpc.Connection {
	if conn, err := net.DialTimeout("tcp", address, CONN_DEADLINE*time.Second); err != nil {
		log.Printf("[Tcp] Connect %s failed:\n>>>> %v", address, err)
		return nil
	} else {
		return &TcpConn{Conn: conn}
	}
}

func Pack(data []byte) (pack []byte) {
	var i, x int
	for x = len(data); x > 0; x >>= 7 {
		i++
	}
	pack = make([]byte, i+len(data))

	i = 0
	for x = len(data); x >= 0x80; x >>= 7 {
		pack[i] = byte(x) | 0x80
		i++
	}
	pack[i] = byte(x)
	copy(pack[i+1:], data)
	return
}

func Extend(buffer []byte, data []byte) []byte {
	m := len(buffer)
	n := m + len(data)
	if n <= cap(buffer) {
		buffer = buffer[:n]
		copy(buffer[m:], data)
		return buffer
	} else {
		buf := make([]byte, n, n*2+cap(buffer))
		copy(buf, buffer)
		copy(buf[m:], data)
		return buf
	}
}

func Unpack(pack []byte) (data []byte, err error) {
	l := len(pack)
	if l <= 0 {
		return
	}
	var x uint
	var length, offset int
	for i, b := range pack {
		if b < 0x80 {
			if i > 9 || i == 9 && b > 1 {
				err = fmt.Errorf("[Network] Unpack length error: %v %v", pack[:i], length)
				return
			}
			length |= int(b) << x
			offset = i + 1
		}
		length |= int(b&0x7f) << x
		x += 7
	}
	length += offset
	if length >= l {
		data = pack[offset:length]
		copy(pack, pack[length:])
		pack = pack[:l-length]
	}
	return
}

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
