from django.shortcuts import render
from datetime import datetime
import jdatetime
from ProductsMachine.models import *
from Products.models import *

def convert_to_jalali(iso_timestamp):
    try:
        dt = datetime.fromisoformat(iso_timestamp.replace('Z', '+00:00'))
        jalali_dt = jdatetime.datetime.fromgregorian(datetime=dt)
        return jalali_dt.strftime('%Y/%m/%d %H:%M:%S')
    except (ValueError, AttributeError):
        return iso_timestamp

def convert_to_unix_timestamp(iso_timestamp):
    try:
        dt = datetime.fromisoformat(iso_timestamp.replace('Z', '+00:00'))
        return int(dt.timestamp())
    except (ValueError, AttributeError, TypeError):
        return iso_timestamp

def index(request):
    context = {
        "products": Products.objects.all(),
        "products_machine": ProductsMachine.objects.all(),
        "products_type": ProductsType.objects.all(),
        "products_key": PRODUCTS_KEY,
        "qr_setting": QR_Settings.objects.first(),
    }
    return render(request,"base/index.html",context)