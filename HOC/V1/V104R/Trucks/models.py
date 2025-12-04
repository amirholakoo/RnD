from django.db import models
import time

'''
class Truck(models.Model):
    """
    Represents a truck used for transportation purposes.

    Attributes:
        id (int): Auto-incrementing primary key for the truck.
        license_number (str): Unique license number of the truck (max length 255).
        driver_name (str, optional): Name of the driver assigned to the truck (max length 255).
        driver_doc (str, optional): doc of the driver assigned to the truck (max length 255).
        phone (str, optional): Phone number of the driver (max length 20).
        status (str): Current status of the truck (choices: 'Free', 'Busy').
        location (str, optional): Current location of the truck (max length 255).
        username (str, optional): username.
    """
    date = models.DateTimeField(default=timezone.now, blank=True)
    status = models.CharField(max_length=10, choices=[('Free', 'Free'), ('Busy', 'Busy')], default='Free')
    location = models.CharField(max_length=255, blank=True, default='Entrance')

    license_number = models.CharField(max_length=255, unique=True)
    driver_name = models.CharField(max_length=255, blank=True)
    driver_doc = models.CharField(max_length=255, blank=True)
    phone = models.CharField(max_length=20, blank=True)

    username = models.CharField(max_length=255, null=False, blank=True)
    logs = models.TextField(blank=True)

    # Meta
    class Meta:
        db_table = 'Truck'

    def __str__(self):
        """
        Returns a human-readable representation of the Truck object.

        Format: "Truck {license_number} - {status}"
        """
        return f"Truck {self.license_number} - {self.status} - {self.driver_name} - {self.location}"

'''

class Truck(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True)
    License_plate_number = models.CharField(max_length=16)
    Truck_type = models.CharField(max_length=255,blank=True,null=True)
    Truck_color = models.CharField(max_length=255,blank=True,null=True)
    status = models.CharField(max_length=10, choices=[('Free', 'Free'), ('Busy', 'Busy')], default='Free')
    location = models.CharField(max_length=255, blank=True, default='Entrance')
    driver_name = models.CharField(max_length=255, blank=True,null=True)
    driver_doc = models.CharField(max_length=255, blank=True,null=True)
    phone = models.CharField(max_length=20, blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)
