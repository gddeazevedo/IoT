import sqlite3
from ..config import database

class Connector:
    @classmethod
    def get_connection(cls):
        return sqlite3.connect(database.db_file)

    # Implementando o contexto
    def __enter__(self):
        self.conn = self.get_connection()
        return self.conn

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.conn:
            self.conn.close()

