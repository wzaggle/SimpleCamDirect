"""Independent position/identity modes; real localhost UDP, no camera access."""
import socket, struct, subprocess, sys, tempfile, threading, unittest, time
from pathlib import Path
EXE=str(Path(sys.argv.pop(1)).resolve())
def frame(kind,payload,sequence=0):return struct.pack('!HHI',kind,len(payload),sequence)+payload
def free_port():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]
class ProbeTest(unittest.TestCase):
    def simulate(self,mode='positions',fault='',initial=1,default_mode=False):
        errors=[];requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(.7);local=free_port()
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        data,peer=server.recvfrom(2048);requests.append(data)
                        self.assertEqual(peer[1],local);self.assertEqual(data,bytes.fromhex('02 00 00 01 00 00 00 00 01'))
                        if fault=='reset_timeout':return
                        reply.sendto(frame(0x0201,b'\x01',123),peer)
                        commands=[(0x0110,'81 09 04 00 FF','90 50 02 FF'),(0x0110,'81 09 04 47 FF','90 50 01 02 03 04 FF'),(0x0110,'81 09 06 12 FF','90 50 0F 0F 0F 0F 00 00 00 01 FF')]
                        if mode=='identity':commands=[(0x0120,'81 09 00 02 FF','90 50 00 01 05 13 01 23 02 FF')]
                        for i,(kind,command,payload) in enumerate(commands):
                            data,again=server.recvfrom(2048);requests.append(data);self.assertEqual(again,peer)
                            self.assertEqual(data,frame(kind,bytes.fromhex(command),initial+i))
                            if fault=='timeout' or (fault=='zoom_timeout' and i==1):return
                            if fault=='standby' and i==0:payload='90 50 03 FF'
                            if fault=='bad_nibble' and i==1:payload='90 50 10 02 03 04 FF'
                            if fault=='visca_error' and i==1:payload='90 60 02 FF'
                            if fault=='noise' and i==1:
                                reply.sendto(b'\x01',peer);reply.sendto(frame(kind,bytes.fromhex(payload),99),peer)
                            reply.sendto(frame(0x0111,bytes.fromhex(payload),initial+i),peer)
                            if fault=='standby' or (fault in ('bad_nibble','visca_error') and i==1):return
                except BaseException as exc:errors.append(exc)
            t=threading.Thread(target=camera,daemon=True);t.start()
            args=[EXE,'127.0.0.1',str(server.getsockname()[1]),'100',str(local),str(initial)]
            if not default_mode:args+=[mode]
            r=subprocess.run(args,cwd=work,capture_output=True,text=True,timeout=3);t.join(2)
            self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            self.assertEqual(len(list(Path(work).glob('SonyUdpProbe-*.log'))),1)
            return r,requests
    def test_positions_mode_excludes_identity(self):
        r,q=self.simulate();self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),4)
        self.assertNotIn('model/ROM inquiry',r.stdout);self.assertIn('zoom position=0x1234 (raw 4660)',r.stdout);self.assertIn('pan=-1; tilt=1',r.stdout)
    def test_identity_only(self):
        r,q=self.simulate('identity');self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),2);self.assertIn('model=0x0513 (SRG-300H)',r.stdout);self.assertNotIn('power inquiry,',r.stdout)
    def test_identity_timeout_then_positions_unaffected(self):
        r,q=self.simulate('identity','timeout');self.assertEqual(r.returncode,1);self.assertEqual(len(q),2)
        r,q=self.simulate();self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),4)
    def test_ten_position_runs(self):
        for _ in range(10):
            r,_=self.simulate();self.assertEqual(r.returncode,0,r.stdout)
    def test_standby_skips_positions(self):
        r,q=self.simulate(fault='standby');self.assertEqual(r.returncode,2);self.assertEqual(len(q),2)
    def test_custom_initial_sequence(self):
        r,_=self.simulate(initial=17);self.assertEqual(r.returncode,0,r.stdout)
    def test_noisy_stale_then_valid(self):
        r,_=self.simulate(fault='noise');self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(r.stdout.count('Ignored:'),2)
    def test_bad_position_nibble(self):
        r,q=self.simulate(fault='bad_nibble');self.assertEqual(r.returncode,1);self.assertEqual(len(q),3)
    def test_position_visca_error(self):
        r,q=self.simulate(fault='visca_error');self.assertEqual(r.returncode,1);self.assertEqual(len(q),3);self.assertIn('Zoom position VISCA error',r.stdout)
    def test_zoom_timeout(self):
        r,q=self.simulate(fault='zoom_timeout');self.assertEqual(r.returncode,1);self.assertEqual(len(q),3)
    def test_reset_timeout(self):
        r,q=self.simulate(fault='reset_timeout');self.assertEqual(r.returncode,1);self.assertEqual(len(q),1)
    def test_invalid_mode_no_transmit(self):
        with tempfile.TemporaryDirectory() as work:
            r=subprocess.run([EXE,'127.0.0.1','52381','100','52381','1','bogus'],cwd=work,capture_output=True,text=True,timeout=2)
            self.assertEqual(r.returncode,1);self.assertIn('Invalid mode',r.stdout);self.assertNotIn('TX  ',r.stdout)

class CompareTest(unittest.TestCase):
    def simulate_compare(self, fault=''):
        errors=[]; requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(2);local=free_port()
            # Literal wire fixtures from the two inspected implementations.
            queries=[
                '01 10 00 05 00 00 00 01 81 09 04 00 FF',
                '01 10 00 05 00 00 00 01 81 09 04 47 FF',
                '01 10 00 05 00 00 00 01 81 09 06 12 FF',
                '01 10 00 06 00 00 00 01 81 09 7E 7E 00 FF',
                '01 00 00 05 00 00 00 01 81 09 04 47 FF',
                '01 00 00 05 00 00 00 01 81 09 06 12 FF']
            replies=['90 50 02 FF','90 50 01 02 03 04 FF','90 50 00 00 00 01 0F 0F 0F 0F FF',
                '90 50 01 02 03 04 00 00 00 00 00 00 00 00 00 FF',
                '90 50 01 02 03 04 FF','90 50 00 00 00 01 0F 0F 0F 0F FF']
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0));first=None
                        for i in range(6):
                            reset,peer=server.recvfrom(2048);requests.append(reset)
                            self.assertEqual(reset,bytes.fromhex('02 00 00 01 00 00 00 00 01'))
                            self.assertEqual(peer[1],local)
                            if first is None:first=peer
                            self.assertEqual(peer,first)
                            if fault=='reset_timeout' and i==1:continue
                            reply.sendto(frame(0x0201,b'\x01'),peer)
                            query,peer=server.recvfrom(2048);requests.append(query)
                            self.assertEqual(query,bytes.fromhex(queries[i]))
                            if fault=='all_timeout' or (fault=='query_timeout' and i==1):continue
                            payload=bytes.fromhex(replies[i])
                            if fault=='bad_block' and i==3:payload=payload[:-1]
                            if fault=='visca_error' and i==1:payload=bytes.fromhex('90 60 02 FF')
                            if fault=='stale' and i==1:reply.sendto(frame(0x0111,payload,99),peer)
                            reply.sendto(frame(0x0111,payload,1),peer)
                except BaseException as exc:errors.append(exc)
            t=threading.Thread(target=camera,daemon=True);t.start()
            r=subprocess.run([EXE,'127.0.0.1',str(server.getsockname()[1]),'60',str(local),'1','compare'],cwd=work,capture_output=True,text=True,timeout=4)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            return r,requests
    def test_default_compare_all_six(self):
        r,q=self.simulate_compare();self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),12);self.assertIn('6/6 inquiries',r.stdout)
    def test_query_timeout_does_not_block_later_cases(self):
        r,q=self.simulate_compare('query_timeout');self.assertEqual(r.returncode,2,r.stdout);self.assertEqual(len(q),12);self.assertIn('5/6 inquiries',r.stdout)
    def test_reset_timeout_skips_only_its_query(self):
        r,q=self.simulate_compare('reset_timeout');self.assertEqual(r.returncode,2,r.stdout);self.assertEqual(len(q),11);self.assertIn('5/6 inquiries',r.stdout)
    def test_all_timeouts_still_finish_summary(self):
        r,_=self.simulate_compare('all_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertIn('0/6 inquiries',r.stdout)
    def test_malformed_block_preserves_other_results(self):
        r,_=self.simulate_compare('bad_block');self.assertEqual(r.returncode,2,r.stdout);self.assertIn('5/6 inquiries',r.stdout)
    def test_visca_error_then_continue(self):
        r,_=self.simulate_compare('visca_error');self.assertEqual(r.returncode,2,r.stdout);self.assertIn('5/6 inquiries',r.stdout)
    def test_stale_reply_then_match(self):
        r,_=self.simulate_compare('stale');self.assertEqual(r.returncode,0,r.stdout);self.assertIn('sequence mismatch',r.stdout)


class ZoomMovementTest(unittest.TestCase):
    def simulate_zoom(self, fault=''):
        import time
        errors=[]; requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(1);local=free_port()
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        def expect(wire):
                            data,peer=server.recvfrom(2048);requests.append(data)
                            self.assertEqual(data,bytes.fromhex(wire));self.assertEqual(peer[1],local)
                            return peer
                        peer=expect('020000010000000001')
                        reply.sendto(frame(0x0201,b'\x01'),peer)
                        peer=expect('011000050000000181090400ff')
                        reply.sendto(frame(0x0111,bytes.fromhex('905003ff' if fault=='standby' else '905002ff'),1),peer);power_time=time.monotonic()
                        if fault=='standby':return
                        peer=expect('011000050000000281090447ff')
                        self.assertGreaterEqual(time.monotonic()-power_time,.095)
                        if fault=='baseline_timeout':return
                        reply.sendto(frame(0x0111,bytes.fromhex('905004000000ff' if fault=='limit' else '905000000000ff'),2),peer);baseline_time=time.monotonic()
                        if fault=='limit':return
                        peer=expect('01000006000000038101040720ff');started=time.monotonic()
                        self.assertGreaterEqual(started-baseline_time,.095)
                        # No drive reply at all: STOP must still be sent on time.
                        peer=expect('01000006000000048101040700ff')
                        self.assertGreaterEqual(time.monotonic()-started,.20)
                        self.assertLess(time.monotonic()-started,.65)
                        if fault in ('stop_timeout','stop_error'):
                            if fault=='stop_error':reply.sendto(frame(0x0111,bytes.fromhex('906102ff'),4),peer)
                            expect('01000006000000058101040700ff');return
                        reply.sendto(frame(0x0111,bytes.fromhex('9051ff'),3),peer) # late drive completion
                        reply.sendto(frame(0x0111,bytes.fromhex('9041ff'),4),peer)
                        reply.sendto(frame(0x0111,bytes.fromhex('9051ff'),4),peer)
                        peer=expect('011000050000000581090447ff')
                        if fault=='final_timeout':return
                        reply.sendto(frame(0x0111,bytes.fromhex('905000000000ff' if fault=='unchanged' else '905000000100ff'),5),peer)
                except BaseException as e:errors.append(e)
            t=threading.Thread(target=camera,daemon=True);t.start()
            r=subprocess.run([EXE,'127.0.0.1',str(server.getsockname()[1]),'100',str(local),'1','zoom-test'],cwd=work,capture_output=True,text=True,timeout=4)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            return r,requests
    def test_changed_position_and_stop_without_drive_reply(self):
        r,q=self.simulate_zoom();self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),6);self.assertIn('position increased',r.stdout)
    def test_unchanged_is_inconclusive(self):
        r,_=self.simulate_zoom('unchanged');self.assertEqual(r.returncode,2,r.stdout)
    def test_standby_no_movement(self):
        r,q=self.simulate_zoom('standby');self.assertEqual(r.returncode,2,r.stdout);self.assertEqual(len(q),2)
    def test_baseline_timeout_no_movement(self):
        r,q=self.simulate_zoom('baseline_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertEqual(len(q),3)
    def test_tele_limit_no_movement(self):
        r,q=self.simulate_zoom('limit');self.assertEqual(r.returncode,2,r.stdout);self.assertEqual(len(q),3)
    def test_stop_timeout_additional_stop_no_query(self):
        r,q=self.simulate_zoom('stop_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertEqual(len(q),6)
    def test_stop_error_additional_stop_no_query(self):
        r,q=self.simulate_zoom('stop_error');self.assertEqual(r.returncode,1,r.stdout);self.assertEqual(len(q),6)
    def test_final_timeout_after_confirmed_stop(self):
        r,_=self.simulate_zoom('final_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertIn('STOP completion received',r.stdout)


class SequenceTwoTest(unittest.TestCase):
    def simulate(self, answers):
        errors=[];requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(1);local=free_port()
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        for header,answer in zip(('0110','0100'),answers):
                            for wire,payload,seq,kind in [
                                ('020000010000000001','01',0,0x0201),
                                ('011000050000000181090400ff','905002ff',1,0x0111),
                                (header+'00050000000281090447ff','905000000102ff',2,0x0111)]:
                                data,peer=server.recvfrom(2048);requests.append(data)
                                self.assertEqual(data,bytes.fromhex(wire));self.assertEqual(peer[1],local)
                                if seq==1:self.assertGreaterEqual(time.monotonic()-reset_ack_time,0.095)
                                if seq==2 and not answer:continue
                                reply.sendto(frame(kind,bytes.fromhex(payload),seq),peer)
                                if seq==0:reset_ack_time=time.monotonic()
                except BaseException as exc:errors.append(exc)
            t=threading.Thread(target=camera,daemon=True);t.start()
            r=subprocess.run([EXE,'127.0.0.1',str(server.getsockname()[1]),'60',str(local),'1','compare-seq2'],cwd=work,capture_output=True,text=True,timeout=3)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            self.assertEqual(len(requests),6)
            self.assertNotIn('81 01 04 07',r.stdout)
            return r
    def test_both_reply(self):
        r=self.simulate((True,True));self.assertEqual(r.returncode,0,r.stdout);self.assertIn('2/2 sequence-2',r.stdout)
    def test_both_timeout(self):
        r=self.simulate((False,False));self.assertEqual(r.returncode,1,r.stdout);self.assertIn('0/2 sequence-2',r.stdout)
    def test_only_command_header_replies(self):
        r=self.simulate((False,True));self.assertEqual(r.returncode,2,r.stdout);self.assertIn('PASS: Zoom 01 00',r.stdout)
    def test_only_inquiry_header_replies(self):
        r=self.simulate((True,False));self.assertEqual(r.returncode,2,r.stdout);self.assertIn('PASS: Zoom 01 10',r.stdout)


class PowerControlTest(unittest.TestCase):
    def simulate(self,fault=''):
        errors=[]; requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(1);local=free_port()
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        data,peer=server.recvfrom(2048);requests.append(data)
                        self.assertEqual(data,bytes.fromhex('020000010000000001'))
                        reply.sendto(frame(0x0201,bytes.fromhex('01')),peer);reset_time=time.monotonic()
                        for seq in (1,2):
                            data,again=server.recvfrom(2048);requests.append(data)
                            self.assertEqual(again,peer)
                            self.assertEqual(data,bytes.fromhex('011000050000000'+str(seq)+'81090400ff'))
                            if seq==1:self.assertGreaterEqual(time.monotonic()-reset_time,.095)
                            if seq==2:self.assertGreaterEqual(time.monotonic()-first_reply_time,.095)
                            if fault=='first_timeout' or (fault=='second_timeout' and seq==2):return
                            if seq==2 and fault=='stale':
                                reply.sendto(frame(0x0111,bytes.fromhex('905002ff'),1),peer)
                            payload='905003ff' if fault=='standby' else '905002ff'
                            if fault=='bad_reply' and seq==2:payload='905000ff'
                            reply.sendto(frame(0x0111,bytes.fromhex(payload),seq),peer)
                            if seq==1:first_reply_time=time.monotonic()
                except BaseException as e:errors.append(e)
            t=threading.Thread(target=camera,daemon=True);t.start()
            r=subprocess.run([EXE,'127.0.0.1',str(server.getsockname()[1]),'100',str(local),'1','power-seq2'],cwd=work,capture_output=True,text=True,timeout=3)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            return r,requests
    def test_default_two_power_inquiries(self):
        r,q=self.simulate();self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(len(q),3);self.assertIn('Test mode: power-seq2',r.stdout)
    def test_second_timeout_preserves_first_pass(self):
        r,q=self.simulate('second_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertEqual(len(q),3);self.assertIn('PASS: power sequence 1',r.stdout);self.assertNotIn('PASS: power sequence 2',r.stdout)
    def test_first_timeout_no_second_request(self):
        r,q=self.simulate('first_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertEqual(len(q),2)
    def test_standby_is_valid_both_times(self):
        r,_=self.simulate('standby');self.assertEqual(r.returncode,0,r.stdout);self.assertIn('sequence 2 = STANDBY',r.stdout)
    def test_stale_sequence_one_does_not_satisfy_second(self):
        r,_=self.simulate('stale');self.assertEqual(r.returncode,0,r.stdout);self.assertIn('sequence mismatch',r.stdout)
    def test_unknown_state_fails(self):
        r,_=self.simulate('bad_reply');self.assertEqual(r.returncode,1,r.stdout);self.assertIn('Unknown power state',r.stdout)


class SpacedZoomTest(unittest.TestCase):
    def simulate(self,fault=''):
        errors=[];requests=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server,tempfile.TemporaryDirectory() as work:
            server.bind(('127.0.0.1',0));server.settimeout(2);local=free_port()
            def camera():
                try:
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reply:
                        reply.bind(('127.0.0.1',0))
                        for header in ('0110','0100'):
                            data,peer=server.recvfrom(2048);requests.append(data)
                            self.assertEqual(data,bytes.fromhex('020000010000000001'))
                            reply.sendto(frame(0x0201,bytes.fromhex('01')),peer);last=time.monotonic()
                            for seq in (1,2,3,4):
                                data,again=server.recvfrom(2048);requests.append(data)
                                self.assertEqual(peer,again);self.assertEqual(peer[1],local)
                                self.assertGreaterEqual(time.monotonic()-last,.095)
                                power=seq in (1,3)
                                expected=('0110' if power else header)+'00050000000'+str(seq)+('81090400ff' if power else '81090447ff')
                                self.assertEqual(data,bytes.fromhex(expected))
                                if fault=='all_timeout' or (fault=='zoom_timeout' and not power):
                                    last=time.monotonic();continue
                                payload=bytes.fromhex('905002ff' if power else '905000000102ff')
                                if fault=='stale' and seq==4:reply.sendto(frame(0x0111,payload,2),peer)
                                reply.sendto(frame(0x0111,payload,seq),peer);last=time.monotonic()
                except BaseException as e:errors.append(e)
            t=threading.Thread(target=camera,daemon=True);t.start()
            r=subprocess.run([EXE,'127.0.0.1',str(server.getsockname()[1]),'70',str(local),'1','spaced-zoom'],cwd=work,capture_output=True,text=True,timeout=4)
            t.join(2);self.assertFalse(t.is_alive())
            if errors:raise errors[0]
            self.assertEqual(len(requests),10)
            return r
    def test_default_both_headers_four_sequences_and_delays(self):
        r=self.simulate();self.assertEqual(r.returncode,0,r.stdout);self.assertIn('Test mode: spaced-zoom',r.stdout);self.assertIn('8/8 spaced',r.stdout)
    def test_zoom_failures_do_not_block_followup_power_or_zoom(self):
        r=self.simulate('zoom_timeout');self.assertEqual(r.returncode,2,r.stdout);self.assertIn('4/8 spaced',r.stdout);self.assertEqual(r.stdout.count('PASS: Power sequence 3'),2)
    def test_all_timeouts_continue(self):
        r=self.simulate('all_timeout');self.assertEqual(r.returncode,1,r.stdout);self.assertIn('0/8 spaced',r.stdout)
    def test_stale_second_zoom_is_ignored(self):
        r=self.simulate('stale');self.assertEqual(r.returncode,0,r.stdout);self.assertEqual(r.stdout.count('sequence mismatch'),2)

if __name__=='__main__':unittest.main()
