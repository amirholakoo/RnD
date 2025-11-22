from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json

@csrf_exempt
def add_truck(requests):
    if requests.method == "POST":
        try:
            data = json.loads(requests.body)
        except Exception as e:
            return JsonResponse({"status": "error", "message": "Invalid request body: "+str(e)}, status=400)
        try:
            truck = Truck(
                name=data.get("name",''),
                License_plate_number=data["License_plate_number"],
                Truck_type=data.get("Truck_type",''),
                Truck_color=data.get("Truck_color",''),
                description=data.get("description",'')
            )
            truck.save()
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)})
        return JsonResponse({"status": "success", "message": "Truck added successfully","truck": truck.id})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)

@csrf_exempt
def edit_truck(requests, truck_id):
    if requests.method == "POST":
        data = json.loads(requests.body)
        try:
            truck = Truck.objects.get(id=truck_id)
            truck.name = data.get("name",'')
            truck.License_plate_number = data["License_plate_number"]
            truck.Truck_type = data.get("Truck_type",'')
            truck.Truck_color = data.get("Truck_color",'')
            truck.description = data.get("description",'')
            truck.save()
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)}, status=400)
        return JsonResponse({"status": "success", "message": "Truck edited successfully","truck": truck.id})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)


@csrf_exempt
def delete_truck(requests, truck_id):
    if requests.method == "POST":
        try:
            truck = Truck.objects.get(id=truck_id)
            truck.Is_Deleted = True
            truck.save()
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)}, status=400)
        return JsonResponse({"status": "success", "message": "Truck deleted successfully"})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)


@csrf_exempt
def list_trucks(requests):
    if requests.method == "GET":
        try:
            trucks = Truck.objects.filter(Is_Deleted=False)
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)}, status=400)
        data = []
        for truck in trucks:
            data.append({
                "id": truck.id,
                "name": truck.name,
                "License_plate_number": truck.License_plate_number,
                "Truck_type": truck.Truck_type,
                "Truck_color": truck.Truck_color,
                "description": truck.description
            })
        return JsonResponse({"status": "success", "message": "Trucks listed successfully","trucks": data})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)

@csrf_exempt
def view_truck(requests, truck_id):
    if requests.method == "GET":
        try:
            truck = Truck.objects.get(id=truck_id)
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)}, status=400)
        return JsonResponse({"status": "success", "message": "Truck viewed successfully","data": {
            "id": truck.id,
            "name": truck.name,
            "License_plate_number": truck.License_plate_number,
            "Truck_type": truck.Truck_type,
            "Truck_color": truck.Truck_color,
            "description": truck.description
        }})
    else:
        return JsonResponse({"status": "error", "message": "Invalid request method"}, status=405)

