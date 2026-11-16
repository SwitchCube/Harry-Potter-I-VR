from probe_session import request_stop

if __name__=='__main__':
    print('HP1-VR-Sitzung wird regulaer geschlossen.' if request_stop() else 'Keine aktive HP1-VR-Testsitzung gefunden.')
