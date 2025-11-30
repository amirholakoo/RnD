from django.shortcuts import render
from .models import *
from django.views.decorators.csrf import csrf_exempt
import json
from django.contrib import messages
from django.shortcuts import redirect

def suppliers(requests):
    suppliers = Supplier.objects.filter(Is_Deleted=False)
    context = {
        'suppliers': suppliers
    }
    return render(requests, "Supplier/suppliers.html", context)

# Create your views here.
@csrf_exempt
def add(requests):
    if requests.method == "POST":
        try:
            supplier = Supplier(
                name=requests.POST.get("name",''),
                phone=requests.POST.get("phone",''),
                address=requests.POST.get("address",''),
                email=requests.POST.get("email",''),
                description=requests.POST.get("description",''),
            )
            supplier.save()
            messages.success(requests, "Supplier added successfully")
            return redirect("suppliers")
        except Exception as e:
            messages.error(requests, "Error adding supplier: "+str(e))
            return redirect("suppliers")
    else:
        return redirect("suppliers")

def edit(requests, supplier_id):
    if requests.method == "POST":
        try:
            supplier = Supplier.objects.get(id=supplier_id)
            supplier.name = requests.POST.get("name",'')
            supplier.phone = requests.POST.get("phone",'')
            supplier.address = requests.POST.get("address",'')
            supplier.email = requests.POST.get("email",'')
            supplier.description = requests.POST.get("description",''),
            supplier.save()
            messages.success(requests, "Supplier updated successfully")
            return redirect("edit", supplier_id=supplier_id)
        except Exception as e:
            messages.error(requests, "Error updating supplier: "+str(e))
            return redirect("edit", supplier_id=supplier_id)
    else:
        return redirect("suppliers")

def delete(requests, supplier_id):
    if requests.method == "POST":
        try:
            supplier = Supplier.objects.get(id=supplier_id)
            supplier.Is_Deleted = True
            supplier.save()
            messages.success(requests, "Supplier deleted successfully")
            return redirect("suppliers")
        except Exception as e:
            messages.error(requests, "Error deleting supplier: "+str(e))
            return redirect("suppliers")
    else:
        return redirect("suppliers")

def view(requests, supplier_id):
    supplier = Supplier.objects.get(id=supplier_id, Is_Deleted=False)
    context = {
        'supplier': supplier
    }
    return render(requests, "Supplier/view.html", context)

