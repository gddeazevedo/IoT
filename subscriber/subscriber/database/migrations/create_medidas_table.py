from ..connector import Connector


def create_medidas_table():
    print("Creating medidas table if it doesn't exist")
    
    with Connector() as conn:
        cursor = conn.cursor()
        cursor.execute('''
		    CREATE TABLE IF NOT EXISTS medidas (
		        id INTEGER PRIMARY KEY AUTOINCREMENT,
		        temperatura REAL NOT NULL,
		        umidade REAL NOT NULL,
		        timestamp INTEGER NOT NULL,
		        id_dispositivo INTEGER NOT NULL,
		        FOREIGN KEY (id_dispositivo) REFERENCES dispositivos(id)
		    )
    	''')
		conn.commit()
    print("Table created or already exists\n")

