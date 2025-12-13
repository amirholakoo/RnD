from django.shortcuts import render
import requests
import threading
import time, os
import json
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.conf import settings
from django.contrib import messages
from django.shortcuts import redirect
from Warehouse.models import Warehouse
from Visions.models import *
"""
/myapp/api/updateWeight1
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/incoming
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus
https://github.com/amirholakoo/ISMv900/blob/main/myapp/models.py
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/LoadingUnloading
"""

def dashboard(request):
    visions = Vision.objects.all()
    vision_detections_24h = []
    for v in visions:
        vision_data = VisionData.objects.filter(vision__id=v.id).filter(CreationDateTime__gte=time.time() - 86400)

        target = {
            "vision": v,
        }
        for vd in vision_data:
            if vd.name:

                target[vd.name] = target.get(vd.name, 0) + vd.count if vd.name != "forklift" else target.get(vd.name, 0) + 1
        vision_detections_24h.append(target)

    visions = Vision.objects.all()
    vision_detections = []
    for v in visions:
        vision_data = VisionData.objects.filter(vision__id=v.id)

        target = {
            "vision": v,
        }
        for vd in vision_data:
            if vd.name:

                target[vd.name] = target.get(vd.name, 0) + vd.count if vd.name != "forklift" else target.get(vd.name, 0) + 1
        vision_detections.append(target)

    context = {
        "vision_detections_24h": vision_detections_24h,
        "vision_detections": vision_detections,
        "warehouses": Warehouse.objects.all(),
    }
    return render(request, 'dashboard/dashboard.html', context)