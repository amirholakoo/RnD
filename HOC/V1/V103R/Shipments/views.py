from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json

def shipments(requests):
    shipments = Shipment.objects.filter(Is_Deleted=False)
    context = {
        'shipments': shipments
    }
    return render(requests, "Shipments/shipments.html", context)

@csrf_exempt
def add(requests):
    if requests.method == "POST":
        try:
            shipment = Shipment(
                name=requests.POST.get("name",''),
                status=requests.POST.get("status",''),
                location=requests.POST.get("location",''),
                receive_date=requests.POST.get("receive_date",''),
                weight1_time=requests.POST.get("weight1_time",''),
                weight2_time=requests.POST.get("weight2_time",''),
                exit_time=requests.POST.get("exit_time",''),
                shipment_type=requests.POST.get("shipment_type",''),
                Truck=requests.POST.get("Truck",''),
                Customer=requests.POST.get("Customer",''),
                Supplier=requests.POST.get("Supplier",''),
                weight1=requests.POST.get("weight1",''),
                unload_location=requests.POST.get("unload_location",''),
                unit=requests.POST.get("unit",''),
                quantity=requests.POST.get("quantity",''),
                quality=requests.POST.get("quality",''),
                penalty=requests.POST.get("penalty",''),
                weight2=requests.POST.get("weight2",''),
                net_weight=requests.POST.get("net_weight",''),
                list_of_reels=requests.POST.get("list_of_reels",''),
                profile_name=requests.POST.get("profile_name",''),
                width=requests.POST.get("width",''),
                sales_id=requests.POST.get("sales_id",''),
                price_per_kg=requests.POST.get("price_per_kg",''),
                total_price=requests.POST.get("total_price",''),
                extra_cost=requests.POST.get("extra_cost",''),
                material=requests.POST.get("material",''),
                vat=requests.POST.get("vat",''),
                invoice_status=requests.POST.get("invoice_status",''),
                payment_status=requests.POST.get("payment_status",''),
                document_info=requests.POST.get("document_info",''),
                comments=requests.POST.get("comments",''),
                cancellation_reason=requests.POST.get("cancellation_reason",''),
                description=requests.POST.get("description",''),
            )
            shipment.save()
            messages.success(requests, "Shipment added successfully")
            return redirect("shipments")
        except Exception as e:
            messages.error(requests, "Error adding shipment: "+str(e))
            return redirect("shipments")
    else:
        return redirect("shipments")

def edit(requests, shipment_id):
    if requests.method == "POST":
        try:
            shipment = Shipment.objects.get(id=shipment_id)
            shipment.name = requests.POST.get("name",'')
            shipment.description = requests.POST.get("description",'')
            shipment.Truck = requests.POST.get("Truck",'')
            shipment.shipment_type = requests.POST.get("shipment_type",'')
            shipment.status = requests.POST.get("status",'')
            shipment.receive_date = requests.POST.get("receive_date",'')
            shipment.exit_time = requests.POST.get("exit_time",'')
            shipment.location = requests.POST.get("location",'')
            shipment.weight1 = requests.POST.get("weight1",'')
            shipment.unload_location = requests.POST.get("unload_location",'')
            shipment.unit = requests.POST.get("unit",'')
            shipment.quantity = requests.POST.get("quantity",'')
            shipment.quality = requests.POST.get("quality",'')
            shipment.penalty = requests.POST.get("penalty",'')
            shipment.weight2 = requests.POST.get("weight2",'')
            shipment.net_weight = requests.POST.get("net_weight",'')
            shipment.list_of_reels = requests.POST.get("list_of_reels",'')
            shipment.profile_name = requests.POST.get("profile_name",'')
            shipment.width = requests.POST.get("width",'')
            shipment.sales_id = requests.POST.get("sales_id",'')
            shipment.price_per_kg = requests.POST.get("price_per_kg",'')
            shipment.total_price = requests.POST.get("total_price",'')
            shipment.extra_cost = requests.POST.get("extra_cost",'')
            shipment.material = requests.POST.get("material",'')
            shipment.vat = requests.POST.get("vat",'')
            shipment.invoice_status = requests.POST.get("invoice_status",'')
            shipment.payment_status = requests.POST.get("payment_status",'')
            shipment.document_info = requests.POST.get("document_info",'')
            shipment.comments = requests.POST.get("comments",'')
            shipment.cancellation_reason = requests.POST.get("cancellation_reason",'')
            shipment.description = requests.POST.get("description",'')
            shipment.save()
            messages.success(requests, "Shipment updated successfully")
            return redirect("edit", shipment_id=shipment_id)
        except Exception as e:
            messages.error(requests, "Error updating shipment: "+str(e))
            return redirect("edit", shipment_id=shipment_id)
    else:
        return redirect("shipments")

def delete(requests, shipment_id):
    if requests.method == "POST":
        try:
            shipment = Shipment.objects.get(id=shipment_id)
            shipment.Is_Deleted = True
            shipment.save()
            messages.success(requests, "Shipment deleted successfully")
            return redirect("shipments")
        except Exception as e:
            messages.error(requests, "Error deleting shipment: "+str(e))
            return redirect("shipments")
    else:
        return redirect("shipments")

def view(requests, shipment_id):
    shipment = Shipment.objects.get(id=shipment_id, Is_Deleted=False)
    context = {
        'shipment': shipment
    }
    return render(requests, "shipments/view.html", context)

