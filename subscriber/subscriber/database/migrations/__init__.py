from .create_dispositivos_table import *
from .create_medidas_table import *


def migrate_all_tables():
    create_dispositivos_table()
    create_medidas_table()

