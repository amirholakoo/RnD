from django.db import models
from django.utils import timezone


class TCP_CONNECTION(models.Model):
    ServerLastUpdate = models.FloatField(null=True, blank=True)
    ClientLastUpdate = models.FloatField(null=True, blank=True)


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


class Materials(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="rolls",db_index=True,null=True,blank=True)
    tank_choices = ((1,"نشاسته"),(2,"سولفات"),(3,"پک"),(4,"ریتنشن"))
    tank = models.IntegerField(choices=tank_choices)
    material_number = models.CharField(max_length=150,db_index=True,null=True,blank=True)
    duration = models.IntegerField(default=0,null=True,blank=True)
    steps_details = models.JSONField(null=True,blank=True)
    Is_Deleted = models.BooleanField(default=False,null=True,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)


class PLC_Logs(models.Model):
    plc = models.ForeignKey(PLC,on_delete=models.CASCADE,related_name="plc_logs",db_index=True,null=True,blank=True)
    Materials = models.ForeignKey(Materials,on_delete=models.CASCADE,related_name="roll_logs",db_index=True,null=True,blank=True)
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
    live_background = models.BooleanField(default=False,blank=True)
    value_max = models.FloatField(default=100,blank=True)
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
    min_value_2 = models.FloatField(null=True,blank=True)
    max_value = models.FloatField(null=True,blank=True)
    max_value_2 = models.FloatField(null=True,blank=True)
    color_max = models.CharField(max_length=20,default='#ff8800',blank=True)
    color_max_2 = models.CharField(max_length=20,default='#ff4444',blank=True)
    color_min = models.CharField(max_length=20,default='#ff4444',blank=True)
    color_min_2 = models.CharField(max_length=20,default='#ff8800',blank=True)
    alert_types = models.JSONField(default=dict,blank=True)
    CreationDateTime = models.DateTimeField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.DateTimeField(verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = timezone.now()
        self.LastUpdate = timezone.now()
        super().save(*args, **kwargs)




TANK_CONFIG = {
    1: {
        "name": "نشاسته",
        "status_key": "s1",
        "weight_key": "W1",
        "steps": {
            0: {
                "name": "خاموش",
                "weight_keys": [],
            },
            1: {
                "name": "آبگیری",
                "weight_keys": [],
            },
            2: {
                "name": "انتظار دستور ساخت",
                "weight_keys": [],
            },
            3: {
                "name": "تزریق مواد 1",
                "weight_keys": [],
            },
            4: {
                "name": "گرم کردن",
                "weight_keys": [],
                "temperature_keys": ["T1"],
            },
            5: {
                "name": "سرد کردن",
                "weight_keys": [],
                "temperature_keys": ["T1"],
            },
            6: {
                "name": "تزریق سولفات",
                "weight_keys": [],
            },
            7: {
                "name": "تزریق رنگ",
                "weight_keys": [],
            },
            8: {
                "name": "تزریق akd",
                "weight_keys": [],
            },
            9: {
                "name": "سرد کردن 2",
                "weight_keys": [],
                "temperature_keys": ["T1"],
            },
            10: {
                "name": "آبگیری هدف",
                "weight_keys": [],
            },
            11: {
                "name": "ارسال",
                "weight_keys": [],
            },
            12: {
                "name": "آبگیری و شستشو",
                "weight_keys": [],
            },
            13: {
                "name": "ارسال آب شستشو",
                "weight_keys": [],
            },
            14: {
                "name": "پایان_عدم اجازه ارسال",
                "weight_keys": [],
            },
        },
    },

    2: {
        "name": "سولفات",
        "status_key": "s2",
        "weight_key": "W2",
        "steps": {
            0: {"name": "خاموش", "weight_keys": []},
            1: {"name": "آبگیری", "weight_keys": []},
            2: {"name": "انتظار دستور ساخت", "weight_keys": []},
            3: {"name": "تزریق مواد", "weight_keys": []},
            4: {"name": "هم زدن", "weight_keys": []},
            5: {"name": "آبگیری هدف", "weight_keys": []},
            6: {"name": "همزدن - عدم اجازه ارسال", "weight_keys": []},
            7: {"name": "ارسال", "weight_keys": []},
            8: {"name": "آبگیری و شستشو", "weight_keys": []},
            9: {"name": "ارسال آب شستشو", "weight_keys": []},
        },
    },

    3: {
        "name": "پک",
        "status_key": "s3",
        "weight_key": "W3",
        "steps": {
            0: {"name": "خاموش", "weight_keys": []},
            1: {"name": "آبگیری", "weight_keys": []},
            2: {"name": "انتظار دستور ساخت", "weight_keys": []},
            3: {"name": "تزریق مواد", "weight_keys": []},
            4: {"name": "هم زدن", "weight_keys": []},
            5: {"name": "آبگیری هدف", "weight_keys": []},
            6: {"name": "همزدن - عدم اجازه ارسال", "weight_keys": []},
            7: {"name": "ارسال", "weight_keys": []},
            8: {"name": "آبگیری و شستشو", "weight_keys": []},
            9: {"name": "ارسال آب شستشو", "weight_keys": []},
        },
    },

    4: {
        "name": "ریتنشن",
        "status_key": "s4",
        "weight_key": "W4",
        "steps": {
            0: {"name": "خاموش", "weight_keys": []},
            1: {"name": "آبگیری", "weight_keys": []},
            2: {"name": "انتظار دستور ساخت", "weight_keys": []},
            3: {"name": "تزریق مواد", "weight_keys": []},
            4: {"name": "تخلیه ریتنشن", "weight_keys": []},
            5: {"name": "همزدن - عدم اجازه ارسال", "weight_keys": []},
            6: {"name": "آبگیری هدف", "weight_keys": []},
            7: {"name": "همزدن", "weight_keys": []},
            8: {"name": "ارسال", "weight_keys": []},
            9: {"name": "آبگیری شستشو", "weight_keys": []},
            10: {"name": "ارسال آب شستشو", "weight_keys": []},
        },
    },
}
