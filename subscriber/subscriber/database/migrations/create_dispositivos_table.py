from ..connector import Connector


def create_dispositivos_table():
    print("Creating dispositivos table if it doesn't exist")
    
    with Connector() as conn:
    	conn.execute('''
		    CREATE TABLE IF NOT EXISTS dispositivos (
		        id INTEGER PRIMARY KEY,
		        lat REAL NOT NULL,
		        long REAL NOT NULL
		    )
		''')
    
    
    print("Table created or already exists\n")

