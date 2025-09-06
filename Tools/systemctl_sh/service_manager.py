#!/usr/bin/env python3
"""
Systemctl Service Manager
A comprehensive tool for managing systemd services from bash and Python scripts.

Usage:
    python3 service_manager.py add-service <service_name> <script_path> [options]
    python3 service_manager.py remove-service <service_name>
    python3 service_manager.py manage-service <service_name> <action>
    python3 service_manager.py list-services
    python3 service_manager.py status <service_name>

Examples:
    # Add a bash script as service
    python3 service_manager.py add-service my-app /path/to/script.sh --user myuser --description "My Application"
    
    # Add a Python script with venv
    python3 service_manager.py add-service my-python-app /path/to/script.py --venv /path/to/venv --user myuser
    
    # Remove a service
    python3 service_manager.py remove-service my-app
    
    # Manage service (start/stop/restart/enable/disable)
    python3 service_manager.py manage-service my-app start
    python3 service_manager.py manage-service my-app enable
"""

import os
import sys
import subprocess
import argparse
import json
import shutil
from pathlib import Path
from typing import Optional, Dict, List, Tuple
import logging

# Configure logging
def setup_logging():
    """Setup logging with fallback for permission issues."""
    handlers = [logging.StreamHandler()]
    
    # Try to add file handler, fallback to user directory if no permission
    try:
        handlers.append(logging.FileHandler('/var/log/service_manager.log'))
    except PermissionError:
        # Fallback to user directory
        log_dir = Path.home() / '.local' / 'log'
        log_dir.mkdir(parents=True, exist_ok=True)
        handlers.append(logging.FileHandler(log_dir / 'service_manager.log'))
    
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=handlers
    )

setup_logging()
logger = logging.getLogger(__name__)

class ServiceManager:
    """Main class for managing systemd services."""
    
    def __init__(self):
        self.systemd_user_dir = Path.home() / '.config' / 'systemd' / 'user'
        self.systemd_system_dir = Path('/etc/systemd/system')
        
        # Use user-accessible directory for services database
        self.services_db_dir = Path.home() / '.local' / 'share' / 'service_manager'
        self.services_db = self.services_db_dir / 'services.json'
        self.services_db_dir.mkdir(parents=True, exist_ok=True)
        
    def load_services_db(self) -> Dict:
        """Load the services database."""
        if self.services_db.exists():
            try:
                with open(self.services_db, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError) as e:
                logger.warning(f"Could not load services database: {e}")
        return {}
    
    def save_services_db(self, services: Dict) -> None:
        """Save the services database."""
        try:
            with open(self.services_db, 'w') as f:
                json.dump(services, f, indent=2)
        except IOError as e:
            logger.error(f"Could not save services database: {e}")
            raise
    
    def validate_script_path(self, script_path: str) -> Tuple[bool, str]:
        """Validate that the script path exists and is executable."""
        path = Path(script_path)
        
        if not path.exists():
            return False, f"Script path does not exist: {script_path}"
        
        if not path.is_file():
            return False, f"Path is not a file: {script_path}"
        
        # Check if it's a Python script or bash script
        if path.suffix == '.py':
            if not os.access(path, os.R_OK):
                return False, f"Python script is not readable: {script_path}"
        else:
            if not os.access(path, os.X_OK):
                return False, f"Script is not executable: {script_path}"
        
        return True, "Valid"
    
    def validate_venv_path(self, venv_path: str) -> Tuple[bool, str]:
        """Validate that the virtual environment path exists and is valid."""
        path = Path(venv_path)
        
        if not path.exists():
            return False, f"Virtual environment path does not exist: {venv_path}"
        
        if not path.is_dir():
            return False, f"Virtual environment path is not a directory: {venv_path}"
        
        # Check for common venv indicators
        python_exe = path / 'bin' / 'python'
        if not python_exe.exists():
            return False, f"Python executable not found in venv: {python_exe}"
        
        return True, "Valid"
    
    def generate_service_name(self, script_path: str, custom_name: Optional[str] = None) -> str:
        """Generate a service name from script path or use custom name."""
        if custom_name:
            return custom_name
        
        script_name = Path(script_path).stem
        # Clean the name for systemd compatibility
        service_name = script_name.replace('_', '-').replace('.', '-')
        return service_name
    
    def create_bash_service_file(self, service_name: str, script_path: str, 
                                user: str, description: str, 
                                working_dir: Optional[str] = None,
                                environment: Optional[Dict[str, str]] = None,
                                use_system: bool = False) -> str:
        """Create a systemd service file for a bash script."""
        
        script_path = Path(script_path).resolve()
        working_dir = working_dir or str(script_path.parent)
        
        service_content = f"""[Unit]
Description={description}
After=network.target
Wants=network.target

[Service]
Type=simple
"""
        
        # Only specify User/Group for system services
        if use_system:
            service_content += f"User={user}\nGroup={user}\n"
        
        service_content += f"""WorkingDirectory={working_dir}
ExecStart={script_path}
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier={service_name}

# Environment variables
"""
        
        if environment:
            for key, value in environment.items():
                service_content += f"Environment={key}={value}\n"
        
        service_content += """
[Install]
WantedBy=default.target
"""
        
        return service_content
    
    def create_python_service_file(self, service_name: str, script_path: str, 
                                  venv_path: str, user: str, description: str,
                                  working_dir: Optional[str] = None,
                                  environment: Optional[Dict[str, str]] = None,
                                  use_system: bool = False) -> str:
        """Create a systemd service file for a Python script with venv."""
        
        script_path = Path(script_path).resolve()
        venv_path = Path(venv_path).resolve()
        python_exe = venv_path / 'bin' / 'python'
        working_dir = working_dir or str(script_path.parent)
        
        service_content = f"""[Unit]
Description={description}
After=network.target
Wants=network.target

[Service]
Type=simple
"""
        
        # Only specify User/Group for system services
        if use_system:
            service_content += f"User={user}\nGroup={user}\n"
        
        service_content += f"""WorkingDirectory={working_dir}
ExecStart={python_exe} {script_path}
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier={service_name}

# Environment variables
Environment=PATH={venv_path}/bin:/usr/local/bin:/usr/bin:/bin
Environment=VIRTUAL_ENV={venv_path}
"""
        
        if environment:
            for key, value in environment.items():
                service_content += f"Environment={key}={value}\n"
        
        service_content += """
[Install]
WantedBy=default.target
"""
        
        return service_content
    
    def install_service(self, service_name: str, service_content: str, 
                       use_system: bool = False) -> bool:
        """Install a systemd service file."""
        try:
            if use_system:
                service_dir = self.systemd_system_dir
                # Need sudo for system-wide installation
                service_file = service_dir / f"{service_name}.service"
                with open(service_file, 'w') as f:
                    f.write(service_content)
                subprocess.run(['sudo', 'systemctl', 'daemon-reload'], check=True)
            else:
                service_dir = self.systemd_user_dir
                service_dir.mkdir(parents=True, exist_ok=True)
                service_file = service_dir / f"{service_name}.service"
                with open(service_file, 'w') as f:
                    f.write(service_content)
                subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
            
            logger.info(f"Service file installed: {service_file}")
            return True
            
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to install service: {e}")
            return False
        except IOError as e:
            logger.error(f"Failed to write service file: {e}")
            return False
    
    def uninstall_service(self, service_name: str, use_system: bool = False) -> bool:
        """Uninstall a systemd service file."""
        try:
            if use_system:
                service_file = self.systemd_system_dir / f"{service_name}.service"
                if service_file.exists():
                    service_file.unlink()
                subprocess.run(['sudo', 'systemctl', 'daemon-reload'], check=True)
            else:
                service_file = self.systemd_user_dir / f"{service_name}.service"
                if service_file.exists():
                    service_file.unlink()
                subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
            
            logger.info(f"Service file removed: {service_file}")
            return True
            
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to uninstall service: {e}")
            return False
        except IOError as e:
            logger.error(f"Failed to remove service file: {e}")
            return False
    
    def run_systemctl_command(self, service_name: str, action: str, 
                             use_system: bool = False) -> Tuple[bool, str]:
        """Run a systemctl command for a service."""
        try:
            if use_system:
                cmd = ['sudo', 'systemctl', action, service_name]
            else:
                cmd = ['systemctl', '--user', action, service_name]
            
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            return True, result.stdout
            
        except subprocess.CalledProcessError as e:
            return False, e.stderr
    
    def add_service(self, service_name: str, script_path: str, 
                   venv_path: Optional[str] = None, user: str = None,
                   description: str = None, working_dir: Optional[str] = None,
                   environment: Optional[Dict[str, str]] = None,
                   use_system: bool = False, auto_start: bool = False) -> bool:
        """Add a new service."""
        
        # Validate inputs
        is_valid, error_msg = self.validate_script_path(script_path)
        if not is_valid:
            logger.error(f"Script validation failed: {error_msg}")
            return False
        
        if venv_path:
            is_valid, error_msg = self.validate_venv_path(venv_path)
            if not is_valid:
                logger.error(f"Virtual environment validation failed: {error_msg}")
                return False
        
        # Set defaults
        if user is None:
            user = os.getenv('USER', 'root')
        
        if description is None:
            description = f"Service for {Path(script_path).name}"
        
        # Load services database
        services = self.load_services_db()
        
        # Check if service already exists
        if service_name in services:
            logger.error(f"Service '{service_name}' already exists")
            return False
        
        # Generate service file content
        if venv_path:
            service_content = self.create_python_service_file(
                service_name, script_path, venv_path, user, description,
                working_dir, environment, use_system
            )
        else:
            service_content = self.create_bash_service_file(
                service_name, script_path, user, description,
                working_dir, environment, use_system
            )
        
        # Install service
        if not self.install_service(service_name, service_content, use_system):
            return False
        
        # Save to database
        services[service_name] = {
            'script_path': str(Path(script_path).resolve()),
            'venv_path': str(Path(venv_path).resolve()) if venv_path else None,
            'user': user,
            'description': description,
            'working_dir': working_dir,
            'environment': environment,
            'use_system': use_system,
            'created_at': subprocess.run(['date', '-Iseconds'], 
                                       capture_output=True, text=True).stdout.strip()
        }
        
        self.save_services_db(services)
        
        # Auto-start if requested
        if auto_start:
            success, output = self.run_systemctl_command(service_name, 'start', use_system)
            if success:
                logger.info(f"Service '{service_name}' started successfully")
            else:
                logger.warning(f"Service '{service_name}' installed but failed to start: {output}")
        
        logger.info(f"Service '{service_name}' added successfully")
        return True
    
    def remove_service(self, service_name: str, use_system: bool = False) -> bool:
        """Remove a service."""
        
        # Load services database
        services = self.load_services_db()
        
        if service_name not in services:
            logger.error(f"Service '{service_name}' not found in database")
            return False
        
        # Stop and disable service first
        self.run_systemctl_command(service_name, 'stop', use_system)
        self.run_systemctl_command(service_name, 'disable', use_system)
        
        # Uninstall service file
        if not self.uninstall_service(service_name, use_system):
            return False
        
        # Remove from database
        del services[service_name]
        self.save_services_db(services)
        
        logger.info(f"Service '{service_name}' removed successfully")
        return True
    
    def manage_service(self, service_name: str, action: str, use_system: bool = False) -> bool:
        """Manage a service (start/stop/restart/enable/disable/status)."""
        
        valid_actions = ['start', 'stop', 'restart', 'enable', 'disable', 'status']
        if action not in valid_actions:
            logger.error(f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}")
            return False
        
        success, output = self.run_systemctl_command(service_name, action, use_system)
        
        if success:
            if action == 'status':
                print(output)
            else:
                logger.info(f"Service '{service_name}' {action}ed successfully")
            return True
        else:
            logger.error(f"Failed to {action} service '{service_name}': {output}")
            return False
    
    def list_services(self) -> None:
        """List all managed services."""
        services = self.load_services_db()
        
        if not services:
            print("No services managed by service_manager")
            return
        
        print(f"{'Service Name':<20} {'Script Path':<40} {'User':<10} {'System':<8} {'Status'}")
        print("-" * 100)
        
        for service_name, service_info in services.items():
            use_system = service_info.get('use_system', False)
            success, status_output = self.run_systemctl_command(service_name, 'is-active', use_system)
            status = status_output.strip() if success else 'unknown'
            
            script_path = service_info['script_path']
            if len(script_path) > 37:
                script_path = '...' + script_path[-34:]
            
            print(f"{service_name:<20} {script_path:<40} {service_info['user']:<10} "
                  f"{'Yes' if use_system else 'No':<8} {status}")
    
    def get_service_status(self, service_name: str, use_system: bool = False) -> None:
        """Get detailed status of a service."""
        services = self.load_services_db()
        
        if service_name not in services:
            logger.error(f"Service '{service_name}' not found in database")
            return
        
        service_info = services[service_name]
        use_system = service_info.get('use_system', use_system)
        
        print(f"Service: {service_name}")
        print(f"Description: {service_info['description']}")
        print(f"Script Path: {service_info['script_path']}")
        if service_info.get('venv_path'):
            print(f"Virtual Environment: {service_info['venv_path']}")
        print(f"User: {service_info['user']}")
        print(f"Working Directory: {service_info.get('working_dir', 'N/A')}")
        print(f"System Service: {'Yes' if use_system else 'No'}")
        print(f"Created: {service_info.get('created_at', 'N/A')}")
        print()
        
        # Get systemctl status
        success, output = self.run_systemctl_command(service_name, 'status', use_system)
        if success:
            print("Systemctl Status:")
            print(output)
        else:
            print(f"Could not get status: {output}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Systemctl Service Manager',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Available commands')
    
    # Add service command
    add_parser = subparsers.add_parser('add-service', help='Add a new service')
    add_parser.add_argument('service_name', help='Name of the service')
    add_parser.add_argument('script_path', help='Path to the script (bash or Python)')
    add_parser.add_argument('--venv', help='Path to Python virtual environment')
    add_parser.add_argument('--user', help='User to run the service as')
    add_parser.add_argument('--description', help='Service description')
    add_parser.add_argument('--working-dir', help='Working directory for the service')
    add_parser.add_argument('--env', action='append', help='Environment variable (KEY=VALUE)')
    add_parser.add_argument('--system', action='store_true', help='Install as system service (requires sudo)')
    add_parser.add_argument('--auto-start', action='store_true', help='Start service after installation')
    
    # Remove service command
    remove_parser = subparsers.add_parser('remove-service', help='Remove a service')
    remove_parser.add_argument('service_name', help='Name of the service to remove')
    remove_parser.add_argument('--system', action='store_true', help='Remove system service')
    
    # Manage service command
    manage_parser = subparsers.add_parser('manage-service', help='Manage a service')
    manage_parser.add_argument('service_name', help='Name of the service')
    manage_parser.add_argument('action', choices=['start', 'stop', 'restart', 'enable', 'disable', 'status'],
                              help='Action to perform')
    manage_parser.add_argument('--system', action='store_true', help='Manage system service')
    
    # List services command
    subparsers.add_parser('list-services', help='List all managed services')
    
    # Status command
    status_parser = subparsers.add_parser('status', help='Get detailed status of a service')
    status_parser.add_argument('service_name', help='Name of the service')
    status_parser.add_argument('--system', action='store_true', help='Check system service')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    manager = ServiceManager()
    
    try:
        if args.command == 'add-service':
            # Parse environment variables
            environment = {}
            if args.env:
                for env_var in args.env:
                    if '=' in env_var:
                        key, value = env_var.split('=', 1)
                        environment[key] = value
                    else:
                        logger.warning(f"Invalid environment variable format: {env_var}")
            
            success = manager.add_service(
                args.service_name,
                args.script_path,
                venv_path=args.venv,
                user=args.user,
                description=args.description,
                working_dir=args.working_dir,
                environment=environment if environment else None,
                use_system=args.system,
                auto_start=args.auto_start
            )
            
        elif args.command == 'remove-service':
            success = manager.remove_service(args.service_name, args.system)
            
        elif args.command == 'manage-service':
            success = manager.manage_service(args.service_name, args.action, args.system)
            
        elif args.command == 'list-services':
            manager.list_services()
            success = True
            
        elif args.command == 'status':
            manager.get_service_status(args.service_name, args.system)
            success = True
        
        return 0 if success else 1
        
    except KeyboardInterrupt:
        logger.info("Operation cancelled by user")
        return 1
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        return 1


if __name__ == '__main__':
    sys.exit(main())