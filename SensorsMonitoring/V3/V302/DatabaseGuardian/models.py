from django.db import models
import time
import jdatetime

class DatabaseConfig(models.Model):
    """تنظیمات کلی سیستم مدیریت دیتابیس"""
    ROTATION_CHOICES = [
        ('yearly', 'سالانه'),
        ('monthly', 'ماهانه'),
        ('manual', 'دستی'),
    ]
    
    rotation_mode = models.CharField(max_length=20, choices=ROTATION_CHOICES, default='yearly', verbose_name='حالت چرخش')
    max_size_mb = models.IntegerField(default=500, verbose_name='حداکثر حجم (مگابایت)')
    max_records = models.IntegerField(default=1000000, verbose_name='حداکثر رکورد')
    auto_rotate = models.BooleanField(default=True, verbose_name='چرخش خودکار')
    keep_logs_count = models.IntegerField(default=0, verbose_name='تعداد لاگ نگهداری برای هر سنسور')
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت", null=True, blank=True)
    LastUpdate = models.FloatField(verbose_name="آخرین آپدیت", null=True, blank=True)

    class Meta:
        verbose_name = 'تنظیمات دیتابیس'
        verbose_name_plural = 'تنظیمات دیتابیس'

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

    @classmethod
    def get_config(cls):
        """دریافت یا ایجاد تنظیمات پیش‌فرض"""
        config, _ = cls.objects.get_or_create(pk=1)
        return config


class DatabaseRegistry(models.Model):
    """ثبت و مدیریت دیتابیس‌های موجود"""
    STATUS_CHOICES = [
        ('active', 'فعال'),
        ('archived', 'آرشیو'),
        ('corrupted', 'خراب'),
    ]
    
    name = models.CharField(max_length=100, unique=True, verbose_name='نام دیتابیس')
    file_path = models.CharField(max_length=500, verbose_name='مسیر فایل')
    year = models.IntegerField(verbose_name='سال', db_index=True)
    month = models.IntegerField(null=True, blank=True, verbose_name='ماه')
    day = models.IntegerField(null=True, blank=True, verbose_name='روز')
    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='active', verbose_name='وضعیت')
    is_current = models.BooleanField(default=False, verbose_name='دیتابیس فعلی')
    size_mb = models.FloatField(default=0, verbose_name='حجم (مگابایت)')
    records_count = models.IntegerField(default=0, verbose_name='تعداد رکورد')
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت", null=True, blank=True)
    LastUpdate = models.FloatField(verbose_name="آخرین آپدیت", null=True, blank=True)

    class Meta:
        verbose_name = 'دیتابیس'
        verbose_name_plural = 'دیتابیس‌ها'
        ordering = ['-year', '-month', '-day']

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

    def __str__(self):
        return f"{self.name} ({self.get_status_display()})"

    @classmethod
    def get_current(cls):
        """دریافت دیتابیس فعلی"""
        return cls.objects.filter(is_current=True).first()

    @classmethod
    def get_active_databases(cls):
        """دریافت همه دیتابیس‌های فعال"""
        return cls.objects.filter(status='active')


class GlobalDatabaseSelection(models.Model):
    """انتخاب دیتابیس‌های فعال برای همه کاربران"""
    selected_databases = models.JSONField(default=list, verbose_name='دیتابیس‌های انتخاب شده')
    view_mode = models.CharField(max_length=20, default='current', verbose_name='حالت نمایش')
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت", null=True, blank=True)
    LastUpdate = models.FloatField(verbose_name="آخرین آپدیت", null=True, blank=True)

    class Meta:
        verbose_name = 'انتخاب دیتابیس'
        verbose_name_plural = 'انتخاب دیتابیس'

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

    @classmethod
    def get_selection(cls):
        """دریافت یا ایجاد انتخاب پیش‌فرض"""
        selection, _ = cls.objects.get_or_create(pk=1)
        return selection

