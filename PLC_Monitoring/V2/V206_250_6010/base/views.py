from django.shortcuts import render
from django.views.decorators.csrf import csrf_exempt
from django.http import JsonResponse
from .models import *


@csrf_exempt
def get_latest_version(request):
    try:
        latest_version = VersionControl.objects.first()
        description_html = latest_version.desc.replace('\n', '<br>') if latest_version.desc else ''
        
        data = {
            'version': latest_version.version,
            'desc': description_html,
        }
        return JsonResponse(data)
        
    except VersionControl.DoesNotExist:
        default_version = "0.1"
        default_description = "اولین نسخه"
        return JsonResponse({
            'version': default_version,
            'desc': default_description.replace('\n', '<br>')
        })

