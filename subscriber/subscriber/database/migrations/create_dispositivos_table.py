from ..connector import Connector


def create_dispositivos_table():
    print("Creating dispositivos table if it doesn't exist")

    with Connector() as conn:
		cursor = conn.cursor()
    	cursor.execute('''
		    CREATE TABLE IF NOT EXISTS dispositivos (
		        id INTEGER PRIMARY KEY,
		        lat REAL NOT NULL,
		        long REAL NOT NULL
		    )
		''')
		conn.commit()
    print("Table created or already exists\n")

