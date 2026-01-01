from django.db import models
import time

class PLC(models.Model):
    device_id = models.CharField(max_length=50,unique=True,db_index=True)
    ip_address = models.CharField(max_length=100,blank=True,null=True)
    location = models.CharField(max_length=100,blank=True,null=True)
    name = models.CharField(max_length=100,blank=True,null=True)
    setting = models.JSONField(null=True,blank=True)
    Is_Known = models.BooleanField(default=False)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    class Meta:
        indexes = [
            models.Index(fields=['device_id']),
            models.Index(fields=['CreationDateTime']),
        ]

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class Rolls(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="rolls",db_index=True,null=True,blank=True)
    plc_setting = models.JSONField(null=True,blank=True)
    roll_number = models.IntegerField(db_index=True,null=True,blank=True)
    Printed_length = models.IntegerField(default=0,null=True,blank=True)
    Paper_breaks = models.IntegerField(default=0,null=True,blank=True)
    Is_Deleted = models.BooleanField(default=False,null=True,blank=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

class Roll_Breaks(models.Model):
    roll = models.ForeignKey(Rolls,on_delete=models.CASCADE,related_name="roll_breaks",db_index=True,null=True,blank=True)
    break_reason = models.CharField(max_length=250,null=True,blank=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class PLC_Logs(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="plc_logs",db_index=True,null=True,blank=True)
    roll = models.ForeignKey(Rolls,on_delete=models.CASCADE,related_name="roll_logs",db_index=True,null=True,blank=True)
    is_running = models.BooleanField(default=False,null=True,blank=True)
    data = models.TextField()
    json_data = models.JSONField(null=True,blank=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class PLC_Keys(models.Model):
    name = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    fa_name = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    key = models.CharField(max_length=100,db_index=True)
    value = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    order_index = models.IntegerField(default=0,db_index=True)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        # Auto-assign next order_index for new keys
        if not self.pk and self.order_index == 0:
            max_order = PLC_Keys.objects.aggregate(models.Max('order_index'))['order_index__max']
            self.order_index = (max_order or 0) + 1
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class ChartExcludedKeys(models.Model):
    """Keys to exclude from roll detail charts"""
    key = models.CharField(max_length=100,unique=True,db_index=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        super().save(*args, **kwargs)