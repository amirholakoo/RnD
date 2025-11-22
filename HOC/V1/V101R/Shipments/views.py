from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json

@csrf_exempt
def add_shipment(requests):
    if requests.method == "POST":
        data = json.loads(requests.body)
        shipment = Shipment(
            name=data.get("name",''),
            description=data.get("description",''),
            Truck=data.get("Truck",''),
            Shipment_type=data.get("Shipment_type",''),
            Shipment_status=data.get("Shipment_status",''),
            Shipment_date=data.get("Shipment_date",''),
            Shipment_time=data.get("Shipment_time",''),
            Shipment_location=data.get("Shipment_location",''),
            Shipment_destination=data.get("Shipment_destination",''),
            Shipment_weight=data.get("Shipment_weight",''),
        )
        shipment.save()
        return JsonResponse({"status": "success", "message": "Shipment added successfully","shipment": shipment.id})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)