from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json
from django.contrib import messages
from django.shortcuts import redirect
from Trucks.models import Truck
from Customer.models import Customer
from Supplier.models import Supplier
from Material.models import Material
from Warehouse.models import Warehouse
from Forklift.models import Forklift_Drivers
def shipments(request):
    shipments = Shipment.objects.filter(Is_Deleted=False)
    context = {
        'shipments': shipments,
    }
    return render(request, "shipments/shipments.html", context)

@csrf_exempt
def add(request):
    if request.method == "POST":
        try:
            shipment = Shipment(
                name=request.POST.get("name"),
                status=request.POST.get("status"),
                location=Warehouse.objects.get(id=int(request.POST.get("location",0))),
                receive_date=request.POST.get("receive_date"),
                weight1_time=request.POST.get("weight1_time"),
                weight2_time=request.POST.get("weight2_time"),
                exit_time=request.POST.get("exit_time"),
                shipment_type=request.POST.get("shipment_type"),
                Truck=Truck.objects.get(id=int(request.POST.get("Truck"))),
                Customer=Customer.objects.get(id=int(request.POST.get("Customer"))) if request.POST.get("Customer") else None,
                Supplier=Supplier.objects.get(id=int(request.POST.get("Supplier"))) if request.POST.get("Supplier") else None,
                weight1=request.POST.get("weight1"),
                unload_location=Warehouse.objects.get(id=int(request.POST.get("unload_location"))) if request.POST.get("unload_location") else None,
                unit=request.POST.get("unit"),
                quantity=request.POST.get("quantity"),
                quality=request.POST.get("quality"),
                penalty=request.POST.get("penalty"),
                weight2=request.POST.get("weight2"),
                net_weight=request.POST.get("net_weight"),
                list_of_reels=request.POST.get("list_of_reels"),
                profile_name=request.POST.get("profile_name"),
                width=request.POST.get("width"),
                sales_id=request.POST.get("sales_id"),
                price_per_kg=request.POST.get("price_per_kg"),
                total_price=request.POST.get("total_price"),
                extra_cost=request.POST.get("extra_cost"),
                material=Material.objects.get(id=int(request.POST.get("material",0))),
                vat=request.POST.get("vat"),
                invoice_status=request.POST.get("invoice_status"),
                payment_status=request.POST.get("payment_status"),
                document_info=request.POST.get("document_info"),
                comments=request.POST.get("comments"),
                cancellation_reason=request.POST.get("cancellation_reason"),
                description=request.POST.get("description"),
            )
            shipment.save()
            messages.success(request, "مرسوله با موفقیت اضافه شد")
            return redirect("shipments")
        except Exception as e:
            print(e)
            messages.error(request, "خطا در اضافه شدن مرسوله: "+str(e))
            return redirect("shipments")
    context = {
        'trucks': Truck.objects.all(),
        'customers': Customer.objects.filter(Is_Deleted=False),
        'suppliers': Supplier.objects.filter(Is_Deleted=False),
        'materials': Material.objects.filter(Is_Deleted=False),
        'warehouses': Warehouse.objects.filter(Is_Deleted=False),
        'forklift_drivers': Forklift_Drivers.objects.filter(Is_Deleted=False),
    }
    return render(request, "shipments/add.html", context)

def edit(request, shipment_id):
    if request.method == "POST":
        try:
            shipment = Shipment.objects.get(id=shipment_id)
            shipment.name = request.POST.get("name",'')
            shipment.description = request.POST.get("description",'')
            if request.POST.get("Truck"):
                shipment.Truck = Truck.objects.get(id=int(request.POST.get("Truck")))
            shipment.shipment_type = int(request.POST.get("shipment_type", 1)) if request.POST.get("shipment_type") else 1
            shipment.status = int(request.POST.get("status", 1)) if request.POST.get("status") else 1
            shipment.receive_date = request.POST.get("receive_date",'')
            shipment.weight1_time = request.POST.get("weight1_time",'')
            shipment.weight2_time = request.POST.get("weight2_time",'')
            shipment.exit_time = request.POST.get("exit_time",'')
            if request.POST.get("location"):
                shipment.location = Warehouse.objects.get(id=int(request.POST.get("location")))
            shipment.weight1 = request.POST.get("weight1",'')
            shipment.unload_location = request.POST.get("unload_location",'')
            shipment.unit = request.POST.get("unit",'')
            shipment.quantity = request.POST.get("quantity",'')
            shipment.quality = request.POST.get("quality",'')
            shipment.penalty = request.POST.get("penalty",'')
            shipment.weight2 = request.POST.get("weight2",'')
            shipment.net_weight = request.POST.get("net_weight",'')
            shipment.list_of_reels = request.POST.get("list_of_reels",'')
            shipment.profile_name = request.POST.get("profile_name",'')
            shipment.width = request.POST.get("width",'')
            shipment.sales_id = request.POST.get("sales_id",'')
            shipment.price_per_kg = request.POST.get("price_per_kg",'')
            shipment.total_price = request.POST.get("total_price",'')
            shipment.extra_cost = request.POST.get("extra_cost",'')
            if request.POST.get("material"):
                shipment.material = Material.objects.get(id=int(request.POST.get("material")))
            shipment.vat = request.POST.get("vat",'')
            shipment.invoice_status = request.POST.get("invoice_status",'')
            shipment.payment_status = request.POST.get("payment_status",'')
            shipment.document_info = request.POST.get("document_info",'')
            shipment.comments = request.POST.get("comments",'')
            shipment.cancellation_reason = request.POST.get("cancellation_reason",'')
            if request.POST.get("Customer"):
                shipment.Customer = Customer.objects.get(id=int(request.POST.get("Customer")))
            else:
                shipment.Customer = None
            if request.POST.get("Supplier"):
                shipment.Supplier = Supplier.objects.get(id=int(request.POST.get("Supplier")))
            else:
                shipment.Supplier = None
            if request.POST.get("unload_location_warehouse"):
                shipment.unload_location = Warehouse.objects.get(id=int(request.POST.get("unload_location_warehouse"))).name
            shipment.save()
            messages.success(request, "مرسوله با موفقیت ویرایش شد")
            return redirect("edit_shipment", shipment_id=shipment_id)
        except Exception as e:
            messages.error(request, "خطا در ویرایش مرسوله: "+str(e))
            return redirect("edit_shipment", shipment_id=shipment_id)
    context = {
        'shipment': Shipment.objects.get(id=shipment_id),
        'trucks': Truck.objects.filter(Is_Deleted=False),
        'customers': Customer.objects.filter(Is_Deleted=False),
        'suppliers': Supplier.objects.filter(Is_Deleted=False),
        'materials': Material.objects.filter(Is_Deleted=False),
        'warehouses': Warehouse.objects.filter(Is_Deleted=False),
    }
    return render(request,"shipments/edit.html", context)

def delete(request, shipment_id):
    try:
        shipment = Shipment.objects.get(id=shipment_id)
        shipment.Is_Deleted = True
        shipment.save()
        messages.success(request, "مرسوله با موفقیت حذف شد")
    except Exception as e:
        messages.error(request, "خطا در حذف مرسوله: "+str(e))
    return redirect("shipments")

def view(request, shipment_id):
    shipment = Shipment.objects.get(id=shipment_id, Is_Deleted=False)
    context = {
        'shipment': shipment
    }
    return render(request, "shipments/view.html", context)

