from ..database.connector import Connector



def create_dispositivo(id_, lat, long_):
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f"insert into dispositivos (id, lat, long) values ({id_}, {lat}, {long_})")
		conn.commit()


def get_dispositivo_by_id(id_):
	with Connector() as conn:
		cursor = conn.cursor()
		cursor.execute(f"select * from dispositivos where id = {id_}")
		dispositivo = cursor.fetchone()

		if dispositivo:
			return dispositivo

		return None

