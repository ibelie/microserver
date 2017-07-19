// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

type HashRing []string

func (h HashRing) Append(node string) HashRing {
	return HashRing(append(h, node))
}

func (h HashRing) Remove(node string) HashRing {
	return nil
}