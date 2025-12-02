import sys
import subscriber
from subscriber.database.migrations import migrate_all_tables


MIGRATE = "migrate"

commands = {
    MIGRATE: migrate_all_tables
}

def main():
	if len(sys.argv) > 1:
		command = sys.argv[1]
		if command in commands.keys():
			commands[command]()
			exit(0)
		exit(1)
		
	subscriber.run()

if __name__ == '__main__':
	main()

