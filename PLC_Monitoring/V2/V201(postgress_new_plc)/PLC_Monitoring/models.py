from django.db import models
from django.utils import timezone

class PLC(models.Model):
    device_id = models.CharField(max_length=50,unique=True,db_index=True)
    ip_address = models.CharField(max_length=100,blank=True,null=True)
    location = models.CharField(max_length=100,blank=True,null=True)
    name = models.CharField(max_length=100,blank=True,null=True)
    setting = models.JSONField(null=True,blank=True)
    Is_Known = models.BooleanField(default=False)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    class Meta:
        indexes = [
            models.Index(fields=['device_id']),
            models.Index(fields=['CreationDateTime']),
        ]

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)


class Rolls(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="rolls",db_index=True,null=True,blank=True)
    plc_setting = models.JSONField(null=True,blank=True)
    roll_number = models.IntegerField(db_index=True,null=True,blank=True)
    Printed_length = models.IntegerField(default=0,null=True,blank=True)
    Paper_breaks = models.IntegerField(default=0,null=True,blank=True)
    Is_Deleted = models.BooleanField(default=False,null=True,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)

    def avg_final_data(self):
        logs = PLC_Logs.objects.filter(roll=self).exclude(json_data__isnull=True).order_by('CreationDateTime')
        if not logs.exists():
            return
        
        last_value_keys = ["mi1"]
        numeric_values = {}
        last_values = {}
        
        for log in logs:
            if not log.json_data:
                continue
            for key, value in log.json_data.items():
                try:
                    float(value)
                except:
                    continue

                try:
                    num_value = float(value)
                    if num_value > 0:
                        if key in last_value_keys:
                            last_values[key] = num_value
                        else:
                            if key not in numeric_values:
                                numeric_values[key] = []
                            numeric_values[key].append(num_value)
                except (ValueError, TypeError):
                    continue
        
        if not numeric_values and not last_values:
            return
        
        if self.plc_setting is None:
            self.plc_setting = {}
        
        for key, values in numeric_values.items():
            if values:
                avg = sum(values) / len(values)
                self.plc_setting[key] = str(int(avg))
        
        for key, value in last_values.items():
            print(key,value)
            self.plc_setting[key] = str(int(value))
        
        self.save()
        

class Roll_Breaks(models.Model):
    roll = models.ForeignKey(Rolls,on_delete=models.CASCADE,related_name="roll_breaks",db_index=True,null=True,blank=True)
    break_reason = models.CharField(max_length=250,null=True,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)


class PLC_Logs(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="plc_logs",db_index=True,null=True,blank=True)
    roll = models.ForeignKey(Rolls,on_delete=models.CASCADE,related_name="roll_logs",db_index=True,null=True,blank=True)
    is_running = models.BooleanField(default=False,null=True,blank=True)
    data = models.TextField()
    json_data = models.JSONField(null=True,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)


class PLC_Keys(models.Model):
    name = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    fa_name = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    key = models.CharField(max_length=100,db_index=True)
    value = models.CharField(max_length=100,db_index=True,null=True,blank=True)
    order_index = models.IntegerField(default=0,db_index=True)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        # Auto-assign next order_index for new keys
        if not self.pk and self.order_index == 0:
            max_order = PLC_Keys.objects.aggregate(models.Max('order_index'))['order_index__max']
            self.order_index = (max_order or 0) + 1
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)


class ChartExcludedKeys(models.Model):
    """Keys to exclude from roll detail charts"""
    key = models.CharField(max_length=100,unique=True,db_index=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        super().save(*args, **kwargs)


class KeyAlertConfig(models.Model):
    """Alert configuration for each key"""
    key = models.CharField(max_length=100,unique=True,db_index=True)
    min_value = models.FloatField(null=True,blank=True)
    max_value = models.FloatField(null=True,blank=True)
    color_max = models.CharField(max_length=20,default='#ff4444',blank=True)
    color_min = models.CharField(max_length=20,default='#ff8800',blank=True)
    alert_types = models.JSONField(default=dict,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)