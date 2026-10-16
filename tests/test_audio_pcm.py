"""Host PCM boundary: independently computed areas, splits and original events."""
import ctypes as C
from pathlib import Path
import subprocess
import unittest
from fractions import Fraction

ROOT=Path(__file__).resolve().parents[1]
class Events(C.Structure):
    _fields_=[('edges',C.c_uint32*2560),('count',C.c_uint32),('duration_cycles',C.c_uint32)]
class Pcm(C.Structure):
    _fields_=[('clock_hz',C.c_uint32),('sample_rate',C.c_uint32),('phase',C.c_uint32),
              ('integral',C.c_int64),('high',C.c_bool)]
def events(edges,duration):
    e=Events();e.edges[:len(edges)]=edges;e.count=len(edges);e.duration_cycles=duration;return e

class AudioPcmTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out=ROOT/'build/audio-pcm-tests';out.mkdir(parents=True,exist_ok=True)
        target=out/'libaudio_pcm.so'
        subprocess.run(['gcc','-std=c17','-O2','-Wall','-Wextra','-Werror','-pedantic',
                        '-fPIC','-shared','-Isrc','src/audio_pcm.c','src/audio.c',
                        'generated/audio_cues.c','-o',str(target)],cwd=ROOT,check=True)
        cls.lib=C.CDLL(str(target))
        cls.lib.pop_audio_pcm_reset.argtypes=[C.POINTER(Pcm),C.c_uint32,C.c_uint32]
        cls.lib.pop_audio_pcm_reset.restype=C.c_bool
        cls.lib.pop_audio_pcm_render.argtypes=[C.POINTER(Pcm),C.POINTER(Events),C.c_uint32,
                                             C.POINTER(C.c_int16),C.c_size_t,C.POINTER(C.c_size_t)]
        cls.lib.pop_audio_pcm_render.restype=C.c_bool
        cls.lib.pop_audio_events.argtypes=[C.POINTER(C.c_uint8),C.c_size_t,C.c_bool,C.POINTER(Events)]
        cls.lib.pop_audio_events.restype=C.c_bool

    def convert(self,state,e,cycles):
        out=(C.c_int16*12000)();n=C.c_size_t()
        self.assertTrue(self.lib.pop_audio_pcm_render(C.byref(state),C.byref(e),cycles,out,len(out),C.byref(n)))
        return list(out[:n.value])

    def test_hand_computed_square_wave_sample_areas(self):
        state=Pcm();self.assertTrue(self.lib.pop_audio_pcm_reset(C.byref(state),8,2))
        self.assertEqual(self.convert(state,events([1,3,4,6],8),8),[0,0])
        self.assertEqual((state.phase,state.integral,state.high),(0,0,False))
        state=Pcm();self.lib.pop_audio_pcm_reset(C.byref(state),8,2)
        self.assertEqual(self.convert(state,events([1],8),8),[3000,6000])

    def test_fractional_sample_split_is_identical_to_unsplit_stream(self):
        whole=Pcm();split=Pcm()
        self.lib.pop_audio_pcm_reset(C.byref(whole),11,4)
        self.lib.pop_audio_pcm_reset(C.byref(split),11,4)
        expected=self.convert(whole,events([1,4,9,12,17],23),23)
        actual=[];offset=0
        for length in [2,3,1,8,9]:
            local=[v-offset for v in [1,4,9,12,17]if offset<=v<offset+length]
            actual+=self.convert(split,events(local,length),length);offset+=length
        self.assertEqual(actual,expected)
        self.assertEqual((split.phase,split.integral,split.high),(whole.phase,whole.integral,whole.high))
        # Independent exact rational integration over output sample intervals.
        rational=[]
        for i in range((23*4)//11):
            start,end=Fraction(i*11,4),Fraction((i+1)*11,4)
            points=[start]+[Fraction(v)for v in [1,4,9,12,17]if start<v<end]+[end]
            area=Fraction(0)
            for left,right in zip(points,points[1:]):
                high=sum(v<=left for v in [1,4,9,12,17])%2
                area+=(right-left)*(6000 if high else -6000)
            rational.append(int(area/(end-start)))
        self.assertEqual(expected,rational)

    def test_real_analytic_sound_has_bounded_samples_and_continuous_tail(self):
        request=(C.c_uint8*3)(5,17,18);e=Events()
        self.assertTrue(self.lib.pop_audio_events(request,3,True,C.byref(e)))
        state=Pcm();self.lib.pop_audio_pcm_reset(C.byref(state),1023000,48000)
        samples=self.convert(state,e,e.duration_cycles)
        self.assertEqual(len(samples),(e.duration_cycles*48000)//1023000)
        self.assertTrue(samples);self.assertTrue(any(-6000<x<6000 for x in samples))
        self.assertTrue(all(-6000<=x<=6000 for x in samples))
        tail=self.convert(state,events([],5000),5000)
        self.assertEqual(len(set(tail[-100:])),1)

    def test_invalid_events_or_capacity_leave_every_output_unchanged(self):
        state=Pcm();self.lib.pop_audio_pcm_reset(C.byref(state),8,2)
        for e,cycles,cap in [(events([3,1],8),8,8),(events([9],8),8,8),
                             (events([],9),8,8),(events([],8),8,1)]:
            before=bytes(state);out=(C.c_int16*8)(*([1234]*8));n=C.c_size_t(99)
            self.assertFalse(self.lib.pop_audio_pcm_render(C.byref(state),C.byref(e),cycles,out,cap,C.byref(n)))
            self.assertEqual((bytes(state),list(out),n.value),(before,[1234]*8,99))

if __name__=='__main__':unittest.main(verbosity=2)
