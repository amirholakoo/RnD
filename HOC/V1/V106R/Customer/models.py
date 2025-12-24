from django.db import models
import time

class Customer(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True,verbose_name="نام")
    phone = models.CharField(max_length=255,blank=True,null=True,verbose_name="شماره تلفن")
    address = models.CharField(max_length=255,blank=True,null=True,verbose_name="آدرس")
    email = models.EmailField(max_length=255,blank=True,null=True,verbose_name="ایمیل")
    description = models.TextField(blank=True,null=True,verbose_name="توضیحات")
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

    def __str__(self):
        return self.name

    class Meta:
        verbose_name = "مشتری"
        verbose_name_plural = "مشتریان"