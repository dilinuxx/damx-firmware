import socket
import sys

###############################################################################################################################################################
### Functions 
###############################################################################################################################################################

# Send a command via a socket to the BlueCtl program
# This takes the form #nnnCMD
# Where nnn is the number of bytes to follow
def SendCommandViaSocket(socket,cmd):
    length = len(cmd)
    cmdout = "#" + str(length).zfill(3) + cmd;
    print (cmdout)
    socket.sendall(cmdout.encode('utf-8'))
    
def SendLaserCurrent(socket,unit,laser,current):
    lineout="SETCURR"
    # Ensure 4 digits in the unit number
    lineout = lineout + str(unit).zfill(4)
    # Ensure 2 digits in the laser number
    lineout = lineout + str(laser).zfill(2)
    # Ensure 4 digits in the current value
    lineout = lineout + str(current).zfill(4)
    print (lineout)
    SendCommandViaSocket(socket,lineout)

def ConnectToDevice(socket,unit):
    lineout="CONDEV"
    # Ensure 4 digits in the unit number
    lineout = lineout + str(unit).zfill(4)
    print (lineout)
    SendCommandViaSocket(socket,lineout)

def ConnectToAllAvailableDevices(socket):
    lineout="CONALL"
    print (lineout)
    SendCommandViaSocket(socket,lineout)
    
def DisconnectFromDevice(socket,unit):    
    lineout="DISDEV"
    # Ensure 4 digits in the unit number
    lineout = lineout + str(unit).zfill(4)
    print (lineout)
    SendCommandViaSocket(socket,lineout)
    
def GetDeviceDetails(socket):
    lineout="HOWCON"
    SendCommandViaSocket(socket,lineout)
    # This command responds with a variable number of lines depending on the number of devices 
    # which are in the BlueCtl2 table
    # So have an infinate loop
    while 1:
        # There are always 41 characters in this functions received responses (except for the last line but that doesn't matter)
        data = s.recv(41)
        testString = data.decode("utf-8")
        # Look for the word 'DONE' in a line and return when this is received
        if "DONE" in testString:
            return
        print('Received', repr(data))
    
def ClearAllLasers(socket,unit):
    SendLaserCurrent(socket,unit,1,0)
    data = socket.recv(1024)
    print('Received from laser # 1 ', repr(data))
    SendLaserCurrent(socket,unit,2,0)
    data = socket.recv(1024)
    print('Received from laser # 2 ', repr(data))
    
###############################################################################################################################################################
### Main Program
###############################################################################################################################################################

controller = 3

# The Host name of the laser control PC
HOST = 'rishi.shef.ac.uk'  
# If using the host name then set HOST_IP to nothing i.e..
HOST_IP = ''
# If not enter the IP address of the device
#HOST_IP = '172.16.65.54'
# TCP/IP network port used
PORT = 51003      

print('Starting ..')  

# Create a socket
try: 
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
    print ("Socket successfully created")
except socket.error as err: 
    print ("socket creation failed with error %s" %(err)) 
    sys.exit()
    

# If needed resolve an IP address of the device
# If HOST_IP is empty then then progrm will try to resolve the host address
if HOST_IP == '':
    # Resolve the host name into a IP address
    try: 
        host_ip = socket.gethostbyname(HOST) 
    except socket.gaierror: 
        # this means could not resolve the host 
        print ("there was an error resolving the host")
        sys.exit() 
else:
    # If we have an IP address then use that
    host_ip = HOST_IP  

# Connect to the laser control PC
print ("Connecting ..")
try: 
    s.connect((host_ip, PORT)) 
except socket.error as err: 
    print ("Can't connect due to error %s" %(err))
    sys.exit()

print ("Connected")

# When first connected the laser control PC responds with 'HELLO'
data = s.recv(1024)
print('Initial : ', repr(data))

# When we send 'PING' to the laser control PC it resonds with 'PONG'
# This is a good way of testing the connection is alive
SendCommandViaSocket(s,"PING")
data = s.recv(1024)
print('Received', repr(data))

# See what version of BlueCtl2 is running on the host
SendCommandViaSocket(s,"BUILD")
data = s.recv(1024)
print('Received', repr(data))

# Clear all lasers on device controller
#ClearAllLasers(s, controller);

# Connect to device controller
#ConnectToDevice(s,controller);
#data = s.recv(1024)
#print('Received', repr(data))

# See which devices this version of BlueCtl2 is programmed to communicate with
# This should also show us that device 0 is connected
#GetDeviceDetails(s)

# Tell Laser 1 on Unit controller to output 1.0 amps
#SendLaserCurrent(s,controller,1,1000)
#data = s.recv(1024)
#print('Received', repr(data))

# Tell Laser 2 on Unit controller to output 1.75 amps
#SendLaserCurrent(s,controller,2,1750)
#data = s.recv(1024)
#print('Received', repr(data))

# Disconnect from device controller
#DisconnectFromDevice(s,controller)
#data = s.recv(1024)
#print('Received', repr(data))

# Look at the device details again which should show us that device 0 is no longer connected
#GetDeviceDetails(s)

# Send 'BYE' to close the socket at the other end
SendCommandViaSocket(s,"BYE")

# All done so close the network connection at this end
s.close



