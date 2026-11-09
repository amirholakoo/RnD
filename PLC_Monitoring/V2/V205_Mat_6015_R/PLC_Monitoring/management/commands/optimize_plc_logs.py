from django.core.management.base import BaseCommand
from django.db import transaction

from PLC_Monitoring.models import PLC_Logs


class Command(BaseCommand):
    help = "Optimize PLC logs and keep only delta changes"

    def handle(self, *args, **options):

        current_state = {}

        total = PLC_Logs.objects.count()

        self.stdout.write(f"Total Logs : {total}")

        updated = 0
        deleted = 0

        batch_size = 1000
        processed = 0

        logs_to_update = []
        ids_to_delete = []

        queryset = (
            PLC_Logs.objects
            .order_by("CreationDateTime")
            .iterator(chunk_size=batch_size)
        )

        for log in queryset:

            processed += 1

            delta = {}

            if log.json_data:

                for key, value in log.json_data.items():

                    if current_state.get(key) != value:
                        delta[key] = value

                    current_state[key] = value

            if delta:

                log.json_data = delta
                log.data = ""
                logs_to_update.append(log)

            else:

                ids_to_delete.append(log.id)

            if len(logs_to_update) >= batch_size:

                with transaction.atomic():

                    PLC_Logs.objects.bulk_update(
                        logs_to_update,
                        ["json_data", "data"]
                    )

                updated += len(logs_to_update)
                logs_to_update.clear()

            if len(ids_to_delete) >= batch_size:

                with transaction.atomic():

                    PLC_Logs.objects.filter(
                        id__in=ids_to_delete
                    ).delete()

                deleted += len(ids_to_delete)
                ids_to_delete.clear()

            if processed % batch_size == 0:

                self.stdout.write(
                    f"{processed}/{total}"
                )

        if logs_to_update:

            PLC_Logs.objects.bulk_update(
                logs_to_update,
                ["json_data", "data"]
            )

            updated += len(logs_to_update)

        if ids_to_delete:

            PLC_Logs.objects.filter(
                id__in=ids_to_delete
            ).delete()

            deleted += len(ids_to_delete)

        self.stdout.write("")
        self.stdout.write("Finished")
        self.stdout.write(f"Updated : {updated}")
        self.stdout.write(f"Deleted : {deleted}")