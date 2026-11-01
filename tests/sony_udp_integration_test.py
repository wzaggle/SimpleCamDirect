"""Real Qt runtime/UDP adapter against a localhost camera, no hardware access.
Build simplecamptz_sony_udp_driver, then pass its path and Sony profile path.
"""
import os, socket, struct, subprocess, sys, tempfile, threading, time, unittest
from pathlib import Path
EXE = str(Path(sys.argv.pop(1)).resolve())
PROFILE = str(Path(sys.argv.pop(1)).resolve())
def frame(kind, seq, payload):
    payload=bytes.fromhex(payload)
    return struct.pack('!HHI',kind,len(payload),seq)+payload
def free_port():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]
class UdpIntegration(unittest.TestCase):
    def run_camera(self,mode='sequence',fault=''):
        errors=[];requests=[];times=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
            server.bind(('127.0.0.1',0));reply.bind(('127.0.0.1',0));server.settimeout(.8)
            local=free_port();last_reply=None
            def camera():
                nonlocal last_reply
                try:
                    while True:
                        try:data,peer=server.recvfrom(2048)
                        except socket.timeout:return
                        now=time.monotonic();requests.append(data);times.append(now)
                        self.assertEqual(peer[1],local)
                        kind,count,seq=struct.unpack('!HHI',data[:8]);payload=data[8:]
                        self.assertEqual(count,len(payload));self.assertEqual(payload[0],0x81 if kind!=0x0200 else 1)
                        if kind==0x0200:
                            self.assertEqual(data,bytes.fromhex('020000010000000001'))
                            if fault=='reset_timeout':return
                            reply.sendto(frame(0x0201,0,'01'),peer);last_reply=time.monotonic();continue
                        stop=payload in [bytes.fromhex(x) for x in ['8101040700ff','8101040800ff','8101060101010303ff']]
                        if last_reply is not None and not stop:self.assertGreaterEqual(now-last_reply,.085)
                        if payload[1]==9:
                            self.assertEqual(kind,0x0110)
                            if fault=='wrong_ip':
                                with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as wrong:
                                    wrong.bind(('127.0.0.2',0));wrong.sendto(frame(0x0111,seq,'905002ff'),peer)
                                return
                            if fault=='timeout' or (fault=='second_timeout' and seq==2):return
                            if mode=='priority':continue
                            value='905002ff' if payload==bytes.fromhex('81090400ff') else '905000000601ff'
                            if fault=='noise':
                                reply.sendto(b'bad',peer)
                                reply.sendto(frame(0x0111,seq+99,value),peer)
                                reply.sendto(frame(0x0100,seq,value),peer)
                            if fault=='control_error':reply.sendto(frame(0x0200,seq,'0f01'),peer);return
                            if fault=='visca_error':reply.sendto(frame(0x0111,seq,'906002ff'),peer);return
                            if fault=='malformed_length':
                                reply.sendto(bytes.fromhex('01110004000000019050ff'),peer);return
                            reply.sendto(frame(0x0111,seq,value),peer);last_reply=time.monotonic()
                        else:
                            self.assertEqual(kind,0x0100)
                            self.assertIn(payload,[bytes.fromhex(x) for x in ['8101040720ff','8101040700ff','8101040802ff','8101040800ff','8101060108080103ff','8101060101010303ff']])
                            if mode=='priority' and not stop:continue
                            if stop:
                                reply.sendto(frame(0x0111,seq-1,'9051ff'),peer)
                            reply.sendto(frame(0x0111,seq,'9041ff'),peer)
                            reply.sendto(frame(0x0111,seq,'9051ff'),peer);last_reply=time.monotonic()
                except BaseException as e:errors.append(e)
            t=threading.Thread(target=camera,daemon=True);t.start()
            result=subprocess.run([EXE,mode,str(server.getsockname()[1]),str(local),PROFILE],capture_output=True,text=True,timeout=7)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            return result,requests,times
    def test_spaced_sequence_and_reply_from_different_port(self):
        r,q,_=self.run_camera();self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual(q,[bytes.fromhex(x) for x in ['020000010000000001','011000050000000181090400ff','011000050000000281090447ff','011000050000000381090400ff','011000050000000481090447ff']])
    def test_stale_and_malformed_packets_do_not_complete_requests(self):
        r,_,_=self.run_camera(fault='noise');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_reset_timeout_no_inquiry(self):
        r,q,_=self.run_camera(fault='reset_timeout');self.assertEqual(r.returncode,1);self.assertEqual(len(q),1)
    def test_inquiry_timeout_stops_current_sequence(self):
        r,q,_=self.run_camera(fault='second_timeout');self.assertEqual(r.returncode,1);self.assertEqual(len(q),3)
    def test_visca_error(self):
        r,_,_=self.run_camera(fault='visca_error');self.assertEqual(r.returncode,1);self.assertIn('VISCA error',r.stdout)
    def test_control_error(self):
        r,_,_=self.run_camera(fault='control_error');self.assertEqual(r.returncode,1);self.assertIn('control error',r.stdout)
    def test_wrong_length_not_accepted(self):
        r,_,_=self.run_camera(fault='malformed_length');self.assertEqual(r.returncode,1)
    def test_runtime_startup_queries_only_zoom(self):
        r,q,_=self.run_camera('runtime-init');self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertEqual(len(q),2);self.assertEqual(q[-1][8:],bytes.fromhex('81090447ff'))
    def test_runtime_zoom_stop_and_readback(self):
        r,q,_=self.run_camera('runtime-zoom');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual([x[8:] for x in q],[bytes.fromhex(x) for x in ['01','81090400ff','8101040720ff','8101040700ff','81090447ff']])
    def test_stop_preempts_silent_movement_and_poll(self):
        r,q,t=self.run_camera('priority');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        stop=next(i for i,x in enumerate(q) if x[8:]==bytes.fromhex('8101040700ff'))
        self.assertLess(t[stop]-t[1],.13)
    def test_wrong_camera_ip_cannot_satisfy_request(self):
        r,_,_=self.run_camera(fault='wrong_ip');self.assertEqual(r.returncode,1)
    def test_disable_sends_best_effort_stop_only_after_motion(self):
        r,q,_=self.run_camera('disable');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual([x[8:] for x in q],[bytes.fromhex(x) for x in ['01','8101040720ff','8101040700ff']])
    def test_focus_disable_sends_focus_stop(self):
        r,q,_=self.run_camera('disable-focus');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual(q[-1][8:],bytes.fromhex('8101040800ff'))
    def test_pan_disable_sends_pan_tilt_stop(self):
        r,q,_=self.run_camera('disable-pan');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual(q[-1][8:],bytes.fromhex('8101060101010303ff'))
    def test_duplicate_camera_ip_rejected(self):
        r,q,_=self.run_camera('duplicate');self.assertEqual(r.returncode,1);self.assertIn('already assigned',r.stdout)
    def test_all_runtime_actions_use_profile_payloads(self):
        errors=[];seen=[];local=free_port()
        expected=['8101040002ff','8101060108080103ff','8101060108080303ff','81010604ff',
                  '8101044702000000ff','81090447ff','8101043803ff','8101040802ff','8101040800ff',
                  '8101043802ff','8101043302ff','8101043303ff','8101043e02ff','8101043e03ff',
                  '8101043f0100ff','8101043f0200ff','81090447ff','8101043f0200ff','81090447ff','8101040003ff']
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server:
            server.bind(('127.0.0.1',0));server.settimeout(1)
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        reset,peer=server.recvfrom(2048);self.assertEqual(reset,bytes.fromhex('020000010000000001'))
                        reply.sendto(frame(0x0201,0,'01'),peer)
                        for seq,wire in enumerate(expected,1):
                            data,again=server.recvfrom(2048);self.assertEqual(again,peer);seen.append(data[8:].hex())
                            self.assertEqual(data,frame(0x0110 if wire.startswith('8109') else 0x0100,seq,wire))
                            if wire.startswith('8109'):reply.sendto(frame(0x0111,seq,'905002000000ff'),peer)
                            else:
                                reply.sendto(frame(0x0111,seq,'9041ff'),peer)
                                # Make completion later: appended zoom must not be sent on ACK.
                                time.sleep(.015)
                                reply.sendto(frame(0x0111,seq,'9051ff'),peer)
                except BaseException as e:errors.append(e)
            t=threading.Thread(target=camera);t.start()
            r=subprocess.run([EXE,'runtime-actions',str(server.getsockname()[1]),str(local),PROFILE],capture_output=True,text=True,timeout=7)
            t.join(2)
            if errors:raise errors[0]
            self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertEqual(seen,expected)
    def test_two_cameras_share_listener_and_match_ip(self):
        errors=[];q=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as b:
            a.bind(('127.0.0.1',0));b.bind(('127.0.0.2',a.getsockname()[1]))
            port=a.getsockname()[1];local=free_port()
            def camera(server,payload):
                try:
                    server.settimeout(1)
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind((server.getsockname()[0],0))
                        data,peer=server.recvfrom(2048);q.append(data)
                        self.assertEqual(data,bytes.fromhex('020000010000000001'))
                        reply.sendto(frame(0x0201,0,'01'),peer)
                        data,again=server.recvfrom(2048);q.append(data)
                        self.assertEqual(again,peer);self.assertEqual(peer[1],local)
                        reply.sendto(frame(0x0111,1,payload),peer)
                except BaseException as e:errors.append(e)
            threads=[threading.Thread(target=camera,args=(a,'905002ff')),threading.Thread(target=camera,args=(b,'905000000102ff'))]
            for t in threads:t.start()
            r=subprocess.run([EXE,'multi',str(port),str(local),PROFILE],capture_output=True,text=True,timeout=4)
            for t in threads:t.join(2)
            if errors:raise errors[0]
            self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertEqual(len(q),4)
    def test_repeated_sessions_release_listener(self):
        for _ in range(3):
            r,_,_=self.run_camera();self.assertEqual(r.returncode,0,r.stdout+r.stderr)
if __name__=='__main__':unittest.main()
