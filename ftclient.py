import socket
import threading
import sys
import queue
import time
import os

connections = []
ext = ""

# validate command-line parameters: server-host, server-port, command, filename, data_port
def usage():
    global ext
#    print("len(sys.argv) is " + str(len(sys.argv)))

    if (len(sys.argv) != 5) and (len(sys.argv) != 6):
        print ("Usage: python3 ftclient.py <server host> <CCP port> "
             + "<CCQ port> <command> [filename]")
        exit()
    CCP_port = sys.argv[2]
    CCQ_port = sys.argv[3]
    

#    print("sysarg1:hostname is " + sys.argv[1])
#    print("sysarg2:CCPPort  is " + sys.argv[2])
#    print("sysarg3:CCQPort  is " + sys.argv[3])
#    print("sysarg4:command  is " + sys.argv[4])

    try: 
        x = int(CCP_port) + 1
        x = int(CCQ_port) + 1
    except:
        print("Usage: Port argument must be integer")
        exit()

    if (int(CCP_port) > 65536) | (int(CCQ_port) > 65536):
        print("Usage: Port numbers are between 1 and 65536")
        exit()

    if (sys.argv[4] != "-l") & (sys.argv[4] != "-g"):
    	print ("Usage: Valid commands are -l and -g [filename]")
    	exit()

    if (sys.argv[4] == "-g"):
        try:
            print("getting " + sys.argv[5])
            dot_position = sys.argv[5].find(".")
            ext = sys.argv[5][dot_position:dot_position + 4]
            print("extension is " + ext)
        except:
            print("Usage: filename must follow -g command")
            exit()

    if (sys.argv[4] == "-l") & (len(sys.argv)==6):
    	print("Ignoring extra parameter.")
usage() # immediate call to above function


def CCP():
    global connections
    # connect to P
    CCP = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print("Connecting to " + sys.argv[1] + " on port " + sys.argv[2])
    CCP.connect((sys.argv[1], int(sys.argv[2])))
    connections.append(CCP);
    
#    print("Connected?")
    
    #prepare data connection Q to accept on user-defined port
    # q = queue.Queue()
    CCQ()

    time.sleep(.1)
    print("P> Messaging server with commands...")
    msg = "" + sys.argv[4] + " " + sys.argv[3]
    if len(sys.argv) == 6:
        msg += " " + sys.argv[5]
    CCP.send(msg.encode())

    print("P> Waiting for response from server...")
#    print("P> len(connections) is " + str(len(connections)))
    while True:
        if len(connections) == 2:
            break

# as it stands, this will only ever be executed for a file not found, and not
#  an invalid command, because the commands are given at the command line and
#  parsed through client-side validation. Invalid commands incur a 'usage'
#  prompt and exit with an error.
#    dataBytes = 0
        # CODE: condition -- check if the socket is still open. If not, terminate.
        # except:
        #     break;
    msg2 = CCP.recv(1024)
    if msg2 is None:
        print("P> Control Connection closed by server or interrupt.")
#    dataBytes = 0
#    dataBytes += len(msg2)
#    print("(INSTR) Datagram received: " + dataBytes + "B total.")
    msgStr = bytes(msg2).decode("utf=8")
    print(msgStr)

    # loop until other DCQThread daemon receives 0 on Q and closes Q's socket.
    while len(connections) == 2: 
        pass

#    print("CCQ connection closed. Terminating CCP.")
    CCP.close()
    exit(0)

# send command to server on P

# If server sends an error message on P, display it

# Listen for server to initiate data connection Q and accept

# If -l command, receive directory display on Q, display directory on-screen

# If valid -g command: 
#	handle duplicate filename error
#	receive file on Q
#	and save file in current directory
#	display transfer complete msg on-screen
#	 or receive on P and display file not found error

# close P connection & terminate

#----------------------------------------------------------------------

def CCQ():
    global connections
    # blocks here waiting
    DCQThread = threading.Thread(target=CCQhandler)
    DCQThread.daemon = True
    DCQThread.start()


def CCQhandler():
    DCQ = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    DCQ.bind((sys.argv[1], int(sys.argv[3])))
    DCQ.listen(1)

    #block for accept
    conn, addr = DCQ.accept()
    
    connections.append(conn)
    dataBytes = 0

# handle -g command
    if len(sys.argv) == 6:
        print("Q> Preparing to receive file.")
        try:
            data = conn.recv(1024)
            if data == 0:
                conn.close()
                connections.remove(conn)
            else:
                dataBytes += len(data)
                newfile = open("new_" + sys.argv[5], "wb")
                while data:
                    newfile.write(data)
                    data = conn.recv(1024)
#                print("Done receiving. Closing file, Q, and removing.\n")
                newfile.close()
                conn.close()
                connections.remove(conn)
#                print("removed.\n")
        finally:
            print("Q> Stream ended.\n")
# handle -l command
    else:
        print("Q> Preparing to receive directory listing:")
        data = 1
        while data: # purpose of loop is to make sure it gets the whole file
            data = conn.recv(1024)
            if data == 0:
                break
            entry = bytes(data).decode("utf=8")
            print(entry)
        print("Q> End of stream.")
        conn.close()
        connections.remove(conn)

CCP()