from django.db import models
import time

class Vision(models.Model):
    name = models.CharField(max_length=255)
    description = models.TextField(blank=True,null=True)
    warehouse = models.ForeignKey("Warehouse.Warehouse", on_delete=models.CASCADE,related_name="vision_warehouse",verbose_name="انبار",null=True,blank=True)
    server_ip = models.CharField(max_length=255,blank=True,null=True)
    vision_id = models.CharField(max_length=255,blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class VisionData(models.Model):
    vision = models.ForeignKey(Vision, on_delete=models.CASCADE,related_name="vision_data",verbose_name="ویژن")
    name = models.CharField(max_length=255,blank=True,null=True)
    count = models.IntegerField(default=0,blank=True,null=True)
    detections_time = models.FloatField(max_length=50,verbose_name="زمان تشخیص",null=True,blank=True)
    enter = models.BooleanField(default=False,verbose_name="ورود",null=True,blank=True)
    exit = models.BooleanField(default=False,verbose_name="خروج",null=True,blank=True)
    data = models.JSONField(blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class OrganizingVisionData(models.Model):
    vision = models.ForeignKey(Vision, on_delete=models.CASCADE,related_name="organizing_vision_data",verbose_name="ویژن",null=True,blank=True)
    class_name = models.CharField(max_length=255,blank=True,null=True,verbose_name="نام")
    Type = models.IntegerField(choices=((1,"تخلیه"),(2,"بارگیری"),(3,"جابجایی"),(4,"ورود"),(5,"خروج"),(6,"جابجایی داخلی")),default=1,blank=True,null=True)
    location = models.ForeignKey("Warehouse.Warehouse", on_delete=models.PROTECT,blank=True,null=True,verbose_name="انبار",related_name="organizing_vision_data_location")
    destenation = models.ForeignKey("Warehouse.Warehouse", on_delete=models.PROTECT,blank=True,null=True,verbose_name="انبار مقصد",related_name="organizing_vision_data_destenation")
    count = models.IntegerField(default=0,blank=True,null=True,verbose_name="تعداد")
    start_time = models.FloatField(max_length=50,verbose_name="زمان شروع",null=True,blank=True)
    end_time = models.FloatField(max_length=50,verbose_name="زمان پایان",null=True,blank=True)
    time = models.CharField(max_length=50,verbose_name="زمان",null=True,blank=True)
    time_text = models.CharField(max_length=50,verbose_name="زمان",null=True,blank=True)
    enter = models.BooleanField(default=False,verbose_name="ورود",null=True,blank=True)
    exit = models.BooleanField(default=False,verbose_name="خروج",null=True,blank=True)
    enter_count = models.IntegerField(default=0,blank=True,null=True,verbose_name="تعداد ورود")
    exit_count = models.IntegerField(default=0,blank=True,null=True,verbose_name="تعداد خروج")
    move_count = models.IntegerField(default=0,blank=True,null=True,verbose_name="تعداد جابجایی")
    shipment = models.ForeignKey("Shipments.Shipment", on_delete=models.PROTECT,blank=True,null=True)
    quantity = models.CharField(max_length=255,blank=True,null=True,verbose_name="مقدار")
    quality = models.CharField(max_length=255,blank=True,null=True,verbose_name="کیفیت")
    forklift_driver_name = models.ForeignKey("Forklift.Forklift_Drivers", on_delete=models.PROTECT,blank=True,null=True,verbose_name="نام راننده")
    mismatch_count = models.IntegerField(default=0,blank=True,null=True,verbose_name="تعداد غیر مطابقت")
    Is_Checked = models.BooleanField(default=False,verbose_name="بررسی شده",null=True,blank=True)
    Is_Deleted = models.BooleanField(default=False)
    comment = models.CharField(max_length=500,blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)