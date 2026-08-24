from django.db import models

class ProductsMachine(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True)
    api = models.CharField(max_length=255,blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.DateTimeField(auto_now_add=True, db_index=True, null=True, blank=True)
    LastUpdate = models.DateTimeField(auto_now=True, null=True, blank=True)
