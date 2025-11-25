from django.db import models
import time
from Trucks.models import Truck

"""
class Shipments(models.Model):
    # Model representing a shipment record.
    # Fields
    date = models.DateTimeField(default=timezone.now, blank=True)
    status = models.CharField(max_length=255, choices=[('Registered', 'Registered'), ('LoadingUnloading', 'LoadingUnloading'), ('LoadedUnloaded', 'LoadedUnloaded'), ('Office', 'Office'), ('Delivered', 'Delivered'), ('Cancelled', 'Cancelled')], null=True)
    location = models.CharField(max_length=255, null=True)
    receive_date = models.DateTimeField(blank=True, null=True)
    entry_time = models.DateTimeField(blank=True, null=True, default=timezone.now)
    weight1_time = models.DateTimeField(blank=True, null=True)
    weight2_time = models.DateTimeField(blank=True, null=True)
    exit_time = models.DateTimeField(blank=True, null=True)

    shipment_type = models.CharField(max_length=255, choices=[('Incoming', 'Incoming'), ('Outgoing', 'Outgoing')], null=True)
    # truck_id = models.ForeignKey(Truck, on_delete=models.SET_NULL, blank=True, null=True)
    license_number = models.CharField(max_length=255,null=True)
    customer_name = models.CharField(max_length=255, null=True)
    supplier_name = models.CharField(max_length=255, null=True)
    weight1 = models.IntegerField(null=True)
    unload_location = models.CharField(max_length=255, null=True)
    unit = models.CharField(max_length=255, null=True)
    quantity = models.IntegerField(null=True)
    quality = models.CharField(max_length=255, null=True)
    penalty = models.CharField(max_length=255, null=True)
    weight2 = models.IntegerField(null=True)
    net_weight = models.CharField(max_length=10, null=True)
    list_of_reels = models.TextField(null=True)
    profile_name = models.CharField(max_length=255, null=True)
    width = models.IntegerField(null=True, blank=True)
    sales_id = models.IntegerField(null=True)
    price_per_kg = models.BigIntegerField(null=True)
    total_price = models.BigIntegerField(null=True)
    extra_cost = models.BigIntegerField(null=True)
    # supplier_id = models.IntegerField(null=True)
    # material_id = models.IntegerField(null=True)
    material_type = models.CharField(max_length=255, null=True)
    material_name = models.CharField(max_length=255, null=True)
    purchase_id = models.ForeignKey('Purchases', on_delete=models.SET_NULL, blank=True, null=True)
    vat = models.IntegerField(null=True)
    invoice_status = models.CharField(max_length=255, choices=[('NA', 'NA'), ('Sent', 'Sent'), ('Received', 'Received')], null=True)
    payment_status = models.CharField(max_length=255, choices=[('Terms', 'Terms'), ('Paid', 'Paid')], null=True)
    document_info = models.TextField(null=True)
    comments = models.TextField(null=True)
    cancellation_reason = models.TextField(null=True)

    username = models.CharField(max_length=255, null=False, blank=True)
    logs = models.TextField(blank=True)

    # Meta
    class Meta:
        db_table = 'Shipments'
        verbose_name = 'Shipment'

    def __str__(self):
        # String representation of the Shipment instance.
        return f"{self.receive_date} - {self.shipment_type} - {self.license_number} - {self.supplier_name} - {self.customer_name} - {self.material_type} - {self.material_name} - {self.status} - {self.location}"

"""
class Shipment(models.Model):
    status_choices = ((1,"Pending"),(2,"In Progress"),(3,"Completed"),(4,"Cancelled"))
    name = models.CharField(max_length=255,blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Truck = models.ForeignKey(Truck, on_delete=models.PROTECT)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

