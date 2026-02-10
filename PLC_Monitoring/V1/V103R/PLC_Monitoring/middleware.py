from PLC_Monitoring.models import *
import time

def check_fake_data(get_response):
    print("Custom middleware initialized.")

    def middleware(request):
        response = get_response(request)
        
        return response

    return middleware