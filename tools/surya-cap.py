import socket,time,sys
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
s.bind(('0.0.0.0',5140)); s.settimeout(2)
out=open(sys.argv[1],'wb'); t=time.time()
while time.time()-t<float(sys.argv[2]):
    try: d,_=s.recvfrom(4096)
    except socket.timeout: continue
    out.write(d); out.flush()
