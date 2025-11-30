from django.shortcuts import render
from .models import *
from django.views.decorators.csrf import csrf_exempt
from django.contrib import messages
from django.shortcuts import redirect

def customers(requests):
    customers = Customer.objects.filter(Is_Deleted=False)
    context = {
        'customers': customers
    }
    return render(requests, "Customer/customers.html", context)

@csrf_exempt
def add(requests):
    if requests.method == "POST":
        try:
            customer = Customer(
                name=requests.POST.get("name",''),
                phone=requests.POST.get("phone",''),
                address=requests.POST.get("address",''),
                email=requests.POST.get("email",''),
                description=requests.POST.get("description",''),
            )
            customer.save()
            messages.success(requests, "Customer added successfully")
            return redirect("customers")
        except Exception as e:
            messages.error(requests, "Error adding customer: "+str(e))
            return redirect("customers")
    else:
        return redirect("customers")
def edit(requests, customer_id):
    if requests.method == "POST":
        try:
            customer = Customer.objects.get(id=customer_id)
            customer.name = requests.POST.get("name",'')
            customer.phone = requests.POST.get("phone",'')
            customer.address = requests.POST.get("address",'')
            customer.email = requests.POST.get("email",'')
            customer.description = requests.POST.get("description",'')
            customer.save()
            messages.success(requests, "Customer updated successfully")
            return redirect("edit", customer_id=customer_id)
        except Exception as e:
            messages.error(requests, "Error updating customer: "+str(e))
            return redirect("edit", customer_id=customer_id)
    else:
        return redirect("customers")

def delete(requests, customer_id):
    if requests.method == "POST":
        try:
            customer = Customer.objects.get(id=customer_id)
            customer.Is_Deleted = True
            customer.save()
            messages.success(requests, "Customer deleted successfully")
            return redirect("customers")
        except Exception as e:
            messages.error(requests, "Error deleting customer: "+str(e))
            return redirect("customers")
    else:
        return redirect("customers")
        
def view(requests, customer_id):
    customer = Customer.objects.get(id=customer_id, Is_Deleted=False)
    context = {
        'customer': customer
    }
    return render(requests, "Customer/view.html", context)