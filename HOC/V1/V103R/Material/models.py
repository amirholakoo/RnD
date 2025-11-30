from django.db import models
import time
'''
class RawMaterial(models.Model):
    """
    Represents a raw material used in production.

    Attributes:
        supplier_name
        material_type (str): Type of the raw material (e.g., cotton, wool, plastic).
        material_name (str): Name of the specific raw material (e.g., Pima cotton, Merino wool).
        description (str, optional): Optional description of the raw material and its properties.
        status (str): Current status of the raw material (e.g., available, low stock, discontinued).
        comments (str, optional): Additional comments or notes about the raw material.
    """
    date = models.DateTimeField(default=timezone.now, blank=True)
    status = models.CharField(max_length=255, default='Active')
    supplier_name = models.CharField(max_length=255, null=True, blank=True)
    material_type = models.CharField(max_length=255)
    material_name = models.CharField(max_length=255)
    description = models.CharField(max_length=255, blank=True)
    comments = models.TextField(blank=True)
    username = models.CharField(max_length=255, null=False, blank=True)
    logs = models.TextField(blank=True)

    # Meta
    class Meta:
        db_table = 'RawMaterial'

    def __str__(self):
        """
        Returns a human-readable representation of the RawMaterial object.

        Format: "{material_name} (ID: {material_id})"
        """
        return f"{self.supplier_name} - {self.material_name} - {self.material_type} - {self.description} - {self.status}"


class MaterialType(models.Model):
    """
    MaterialType model represents different types of materials supplied by suppliers.

    Fields:
    - id: Auto-incrementing primary key.
    - supplier_name: Name of the supplier. Can be null or blank.
    - material_type: Type of material. Required field.
    - username: Username associated with the material type. Can be null or blank.
    """
    status = models.CharField(max_length=50, null=True, blank=True, default='Active')
    date = models.DateTimeField(default=timezone.now, blank=True)
    supplier_name = models.CharField(max_length=255, null=True, blank=True)
    # Ensure material_type is always provided to avoid empty entries
    material_type = models.CharField(max_length=255)
    username = models.CharField(max_length=255, null=False, blank=True)
    logs = models.TextField(blank=True)

    # Meta
    class Meta:
        db_table = 'MaterialType'

    def __str__(self):
        """
        Returns a string representation of the MaterialType object, including the supplier name, material type, and username.
        This method is used to display a human-readable representation of the object.
        """
        return f"{self.supplier_name} - {self.material_type} - {self.status}"
'''
class Material(models.Model):
    name = models.CharField(max_length=255)
    description = models.CharField(max_length=255,blank=True,null=True)
    Is_Active = models.BooleanField(default=True)
    supplier_name = models.CharField(max_length=255, null=True, blank=True)
    material_type = models.ForeignKey("MaterialType", on_delete=models.PROTECT)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class MaterialType(models.Model):
    supplier_name = models.CharField(max_length=255, null=True, blank=True)
    material_type = models.CharField(max_length=255)
    description = models.CharField(max_length=255,blank=True,null=True)
    Is_Active = models.BooleanField(default=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)