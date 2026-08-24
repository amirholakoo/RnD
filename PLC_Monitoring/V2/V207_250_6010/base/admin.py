from django.contrib import admin
from .models import VersionControl, VisitorLog

admin.site.register(VersionControl)

@admin.register(VisitorLog)
class VisitorLogAdmin(admin.ModelAdmin):

    list_per_page = 100

    ordering = ["-created_at"]

    date_hierarchy = "created_at"

    list_display = (
        "created_at",
        "ip",
        "method",
        "short_path",
        "status",
        "duration",
        "browser",
        "os",
        "device",
        "device_type",
        "language",
        "short_referrer",
        "short_session",
    )

    list_filter = (
        "method",
        "status",
        "browser",
        "os",
        "device",
        "is_mobile",
        "is_tablet",
        "is_pc",
        "is_bot",
        "created_at",
    )

    search_fields = (
        "ip",
        "path",
        "user_agent",
        "session_key",
        "referer",
    )

    readonly_fields = [
        f.name
        for f
        in VisitorLog._meta.fields
    ]

    def has_add_permission(
        self,
        request,
    ):
        return False

    @admin.display(description="URL")
    def short_path(
        self,
        obj,
    ):
        return (
            obj.path[:60]
            if obj.path
            else "-"
        )

    @admin.display(
        description="Time(ms)"
    )
    def duration(
        self,
        obj,
    ):
        return obj.duration_ms

    @admin.display(
        description="Type"
    )
    def device_type(
        self,
        obj,
    ):

        if obj.is_bot:
            return "BOT"

        if obj.is_mobile:
            return "Mobile"

        if obj.is_tablet:
            return "Tablet"

        if obj.is_pc:
            return "Desktop"

        return "-"

    @admin.display(
        description="Referrer"
    )
    def short_referrer(
        self,
        obj,
    ):
        return (
            obj.referer[:30]
            if obj.referer
            else "-"
        )

    @admin.display(
        description="Session"
    )
    def short_session(
        self,
        obj,
    ):
        return (
            obj.session_key[:10]
            if obj.session_key
            else "-"
        )