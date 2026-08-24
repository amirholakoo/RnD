from django.db import models
import time
class VersionControl(models.Model):
    version = models.CharField(max_length=6,default="0",verbose_name="نسخه نرم افزار")
    desc = models.TextField(blank=True,null=True)
    CreationDateTime = models.CharField(max_length=50,default=time.time(),verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.CharField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)
        


class VisitorLog(models.Model):

    created_at = models.DateTimeField(
        auto_now_add=True
    )

    ip = models.GenericIPAddressField(
        null=True,
        blank=True
    )

    method = models.CharField(
        max_length=10
    )

    path = models.TextField()

    status = models.IntegerField()

    duration_ms = models.IntegerField()

    referer = models.TextField(
        blank=True
    )

    session_key = models.CharField(
        max_length=64,
        blank=True
    )

    language = models.CharField(
        max_length=100,
        blank=True
    )

    user_agent = models.TextField()

    browser = models.CharField(
        max_length=100,
        blank=True
    )

    os = models.CharField(
        max_length=100,
        blank=True
    )

    device = models.CharField(
        max_length=100,
        blank=True
    )

    is_mobile = models.BooleanField(
        default=False
    )

    is_tablet = models.BooleanField(
        default=False
    )

    is_pc = models.BooleanField(
        default=False
    )

    is_bot = models.BooleanField(
        default=False
    )

    class Meta:
        ordering = ["-created_at"]

    def __str__(self):
        return f"{self.created_at}"