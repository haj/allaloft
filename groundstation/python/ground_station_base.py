
import os
from optparse import OptionParser
import io
import time
import random
import thread
import sys
from smtp_stuff import sendMail 
from imap_stuff import checkMessages
import datetime
import string
from time import gmtime, strftime


user = ''
recipient = ''
incoming_server = ''
outgoing_server = ''
password = ''
imei = 0

email_enabled = False
ip_enabled = False
http_post_enabled = False

COMMAND_GET_POS = 0

def send_mo_email(fcu_name,msg):

    global email
    global incoming_server
    global outgoing_server
    global password
    global imei

    #put together body
    body = ''
    
    #subject
    subject = '%d' % imei

    #message is included as an attachment
    attachment = 'msg.sbd'
    fd = open(attachment, 'wb')
    fd.write(msg)
    fd.close()
    
    sendMail(subject, body, user, recipient, password, outgoing_server, attachment)
    
def email_check_task(name):
        
    #check e-mail for messages
    while(1):
        #print 'Checking email'
        msg,subject,received_msg,unread_msgs  = checkMessages(incoming_server,user,password)
        if received_msg:
            print "I should parse data here"
            
        time.sleep(1.0)
        
def GET_POS(args):
    print "Do something useful here"
    value = args[0]
    
    #TODO: we haven't actuallly implemented anything yet
    
def process_cmd(cmd_str):
    
    #split up the string  by space
    cmd_args = cmd_str.split(' ')
    
    #caps on CLI input
    cmd_args[0] = cmd_args[0].upper()
    if(len(cmd_args) > 1):
        args = cmd_args[1:]
    else:
        args = []
        
    possibles = globals().copy()
    possibles.update(locals())
    method = possibles.get(cmd_args[0]) 
    if not method:
         print("Method %s not implemented" % cmd_args[0])
    else:
        method(args)

def main():
    
    global user
    global recipient 
    global incoming_server
    global outgoing_server
    global password
    
    global email_enabled
    global ip_enabled
    global http_post_enabled

    parser = OptionParser()
    parser.add_option("-p", "--passwd", dest="passwd", action="store", help="Password", metavar="PASSWD")
    parser.add_option("-u", "--user", dest="user", action="store", help="E-mail account username", metavar="USER")
    parser.add_option("-r", "--recipient", dest="recipient", action="store", help="Destination e-mail address.", metavar="USER")
    parser.add_option("-i", "--in_srv", dest="in_srv", action="store", help="Incoming e-mail server url", metavar="IN_SRV")
    parser.add_option("-o", "--out_srv", dest="out_srv", action="store", help="Outoging e-mail server", metavar="OUT_SRV")
    parser.add_option("-m", "--mode", dest="mode", action="store", help="Mode: EMAIL,HTTP_POST,IP,NONE", default="NONE", metavar="MODE")
    parser.add_option("-I", "--imei", dest="IMEI",action="store",help="IMEI of target modem.",metavar="IMEI")

    (options, args) = parser.parse_args()
    
    #check for valid arguments
    if options.mode == "EMAIL":
        if options.passwd is None  or options.user is None or options.recipient is None or options.in_srv is None or options.out_srv is None:
            print 'If you want to use e-mail, you must specify in/out servers, user, password, and recipient address.'
            sys.exit()
        else:
            email_enabled = True
    elif options.mode == "HTTP_POST":
        print 'Not implemented yet'
        sys.exit()
    elif options.mode == "IP":
        print 'Not implemented yet'
        sys.exit()
    else:
        print "No valid mode specified"
        sys.exit()

    user = options.user
    recipient = options.recipient
    incoming_server = options.in_srv
    outgoing_server = options.out_srv
    password = options.passwd
    imei = options.imei
    
    #spawn task to monitor email for incoming messages
    thread.start_new_thread ( email_check_task, ( "Thread-1" , ) )
    rx_buffer = ''
    
    while(1):
        "Enter 'x' to exit"
        cmd_str = raw_input("# ")
        if cmd_str == 'x':
            break
        if not cmd_str == '':
            process_cmd(cmd_str)
            
    print "Exiting application."
             

if __name__ == '__main__':
    main()



