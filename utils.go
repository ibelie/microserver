// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

package microserver

func EntityByteSize(i ruid.RUID, k ruid.RUID, t string) int {
	return tygo.SizeVarint(i) + tygo.SizeVarint(k) + tygo.SizeVarint(EntityType(t))
}

func EntitySerialize(i ruid.RUID, k ruid.RUID, t string, output *tygo.ProtoBuf) {
	output.WriteVarint(i)
	output.WriteVarint(k)
	output.WriteVarint(EntityType(t))
}

func EntityDeserialize(input *tygo.ProtoBuf) (i ruid.RUID, k ruid.RUID, t string, err error) {
	for !input.ExpectEnd() {
		var tag int
		var cutoff bool
		if tag, cutoff, err = input.ReadTag(127); err == nil && cutoff && tag == 10 { // MAKE_TAG(1, WireBytes=2)
			var buffer []byte
			if buffer, err = input.ReadBuf(); err == nil {
				var ii, kk, tt uint64
				tmpi := &tygo.ProtoBuf{Buffer: buffer}
				if i, err = tmpi.ReadVarint(); err != nil {
					return
				} else if k, err = tmpi.ReadVarint(); err != nil {
					return
				} else if t, err = tmpi.ReadVarint(); err != nil {
					return
				}
				i = ii
				k = kk
				t = EntityName(tt)
			}
			return
		} else if err = input.SkipField(tag); err != nil {
			return
		}
	}
	return
}
