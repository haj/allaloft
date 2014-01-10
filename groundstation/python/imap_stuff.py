
#v+
#!/usr/bin/env python

import imaplib
import email
import imaplib

def checkMessages(incoming_server, user, password):
    has_data = False
    subject = ''
    obj = imaplib.IMAP4_SSL(incoming_server, '993')
    obj.login(user, password)
    obj.select('Inbox')  
    typ ,data = obj.search(None,'UnSeen')
    string = data[0]
    msg_count = len(string.split(' '))
    index = string.find(' ')
    if not index == -1:
        string = string[:string.find(' ')]
    if(msg_count >= 1 and len(string) > 0):
        obj.store(string,'+FLAGS','\Seen')
        typ, data = obj.fetch(string, '(RFC822)')
        
        #TODO: filter by imei subject
        
        #get attachment
        text = data[0][1]
        msg = email.message_from_string(text)
        for part in msg.walk():
            if part.get_content_maintype() == 'multipart':
                continue
            if part.get('Content-Disposition') is None:
                continue
            filename = part.get_filename()
            
            data = part.get_payload(decode=True)
            has_data = True
            if not data:
                has_data = False
                data = []
                subject = ''
                msg = ''
                continue
        subject = msg['Subject']

    return data,subject,has_data,max(msg_count - 1,0)
    
#def main():
#    checkMessages('imap.gmail.com','jmalsbury.personal@gmail.com','sweet525',0)
    
#if __name__ == '__main__':
#    main()
