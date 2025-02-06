#!/usr/bin/env python3
#
#    Copyright (c) 2022 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

from os.path import exists
import os
import sys
import json
import secrets
import argparse
import subprocess
import logging as log
import base64
from collections import namedtuple
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.serialization import load_der_private_key


HEX_PREFIX = "hex:"

class FactoryDataGenerator:
    """
    Class to generate factory data from given arguments and generate a JSON file

    """

    def __init__(self, arguments) -> None:
        """
        Args:
            arguments (any):All input arguments parsed using ArgParse
        """
        self._args = arguments
        self._factory_data = list()
        self._user_data = dict()

    def generate_json(self):
        """
        This function generates JSON data, .json file and validates it.

        To validate generated JSON data a scheme must be provided within script's arguments.

        - In the first part, if the rotating device id unique id has been not provided
          as an argument, it will be created.
        - If user-provided passcode and Spake2+ verifier have been not provided
          as an argument, it will be created using an external script
        - Passcode is not stored in JSON by default. To store it for debugging purposes, add --include_passcode argument.
        - Validating output JSON is not mandatory, but highly recommended.

        """
        try:
            json_file = open(self._args.output, "w+")
        except FileNotFoundError:
            print("Cannot create JSON file in this location: {}".format(self._args.output))
            sys.exit(-1)
        with json_file:
            # serialize data
            self._add_entry("version", self._args.version)
            self._add_entry("sn", self._args.sn)
            self._add_entry("vendor_id", self._args.vendor_id)
            self._add_entry("product_id", self._args.product_id)
            self._add_entry("vendor_name", self._args.vendor_name)
            self._add_entry("product_name", self._args.product_name)
            self._add_entry("product_label", self._args.product_label)
            self._add_entry("product_url", self._args.product_url)
            self._add_entry("part_number", self._args.part_number)
            self._add_entry("date", self._args.date)
            self._add_entry("hw_ver", self._args.hw_ver)
            self._add_entry("hw_ver_str", self._args.hw_ver_str)
            
            if self._args.user:
                self._add_entry("user", self._user_data)

            factory_data_dict = dict(self._factory_data)

            json_object = json.dumps(factory_data_dict)
            # is_json_valid = True
            json_file.write(json_object)

    def _add_entry(self, name: str, value: any):
        """ Add single entry to list of tuples ("key", "value") """
        if(isinstance(value, bytes) or isinstance(value, bytearray)):
            value = HEX_PREFIX + value.hex()
        if value or (isinstance(value, int) and value == 0):
            log.debug("Adding entry '{}' with size {} and type {}".format(name, sys.getsizeof(value), type(value)))
            self._factory_data.append((name, value))

    def _process_der(self, path: str):
        log.debug("Processing der file...")
        try:
            with open(path, 'rb') as f:
                data = f.read()
                return data
        except IOError as e:
            log.error(e)
            raise e


def main():
    parser = argparse.ArgumentParser(description="NrfConnect Factory Data NVS generator tool")

    def allow_any_int(i): return int(i, 0)
    def base64_str(s): return base64.b64decode(s)

    mandatory_arguments = parser.add_argument_group("Mandatory keys", "These arguments must be provided to generate JSON file")
    optional_arguments = parser.add_argument_group(
        "Optional keys", "These arguments are optional and they depend on the user-purpose")
    parser.add_argument("-s", "--schema", type=str,
                        help="JSON schema file to validate JSON output data")
    parser.add_argument("-o", "--output", type=str, required=True,
                        help="Output path to store .json file, e.g. my_dir/output.json")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Run this script with DEBUG logging level")
    parser.add_argument("--include_passcode", action="store_true",
                        help="Include passcode in factory data. By default, it is used only for generating Spake2+ verifier.")
    parser.add_argument("--overwrite", action="store_true",
                        help="If output JSON file exist this argument allows to generate new factory data and overwrite it.")
    # Json known-keys values
    # mandatory keys
                                            
    mandatory_arguments.add_argument("--version", type=str, required=True,
                                     help="[ascii string] software version string.")
    mandatory_arguments.add_argument("--sn", type=str, required=True,
                                     help="[ascii string] Serial number of a device which can be used to identify \
	                                        the serial number field in the Matter certificate structure. \
	                                        Maximum length of serial number is 20 bytes. \
	                                        Strings longer than 20 bytes will be declined in script")
    optional_arguments.add_argument("--vendor_id", type=allow_any_int,
                                     help="[int | hex int] Provide Vendor Identification Number")
    optional_arguments.add_argument("--product_id", type=allow_any_int,
                                     help="[int | hex int] Provide Product Identification Number")
    mandatory_arguments.add_argument("--vendor_name", type=str, required=True,
                                     help="[string] provide human-readable vendor name")
    optional_arguments.add_argument("--product_name", type=str,
                                     help="[string] provide human-readable product name")
    optional_arguments.add_argument("--date", type=str, required=True,
                                     help="[ascii string] Provide manufacturing date \
                                            A manufacturing date specifies the date that the Node was manufactured. \
	                                        Used format for providing a manufacturing date is ISO 8601 e.g. YYYY-MM-DD.")
    mandatory_arguments.add_argument("--hw_ver", type=str, required=True,
                                     help="[int | hex int] Provide hardware version in int format.")
    optional_arguments.add_argument("--hw_ver_str", type=str,
                                     help="[ascii string] Provide hardware version in string format.")
    optional_arguments.add_argument("--spake2_it", type=allow_any_int,
                                     help="[int | hex int] Provide Spake2+ iteration count.")
    optional_arguments.add_argument("--spake2_salt", type=base64_str,
                                     help="[base64 string] Provide Spake2+ salt.")
    optional_arguments.add_argument("--discriminator", type=allow_any_int,
                                     help="[int] Provide BLE pairing discriminator. \
                                     A 12-bit value matching the field of the same name in \
                                     the setup code. Discriminator is used during a discovery process.")

    # optional keys
    optional_arguments.add_argument("--product_url", type=str,
                                    help="[string] provide link to product-specific web page")
    optional_arguments.add_argument("--product_label", type=str,
                                    help="[string] provide human-readable product label")
    optional_arguments.add_argument("--part_number", type=str,
                                    help="[string] provide human-readable product number")
    optional_arguments.add_argument("--chip_cert_path", type=str,
                                    help="Generate DAC and PAI certificates instead giving a path to .der files. This option requires a path to chip-cert executable."
                                    "By default you can find chip-cert in connectedhomeip/src/tools/chip-cert directory and build it there.")
    optional_arguments.add_argument("--dac_cert", type=str,
                                    help="[.der] Provide the path to .der file containing DAC certificate.")
    optional_arguments.add_argument("--dac_key", type=str,
                                    help="[.der] Provide the path to .der file containing DAC keys.")
    optional_arguments.add_argument("--generate_rd_uid", action="store_true",
                                    help="Generate a new rotating device unique ID, print it out to console output and store it in factory data.")
    optional_arguments.add_argument("--dac_key_password", type=str,
                                    help="Provide a password to decode dac key. If dac key is not encrypted do not provide this argument.")
    optional_arguments.add_argument("--pai_cert", type=str,
                                    help="[.der] Provide the path to .der file containing PAI certificate.")
    optional_arguments.add_argument("--rd_uid", type=str,
                                    help="[hex string] [128-bit hex-encoded] Provide the rotating device unique ID. If this argument is not provided a new rotating device id unique id will be generated.")
    optional_arguments.add_argument("--passcode", type=allow_any_int,
                                    help="[int | hex] Default PASE session passcode. (This is mandatory to generate Spake2+ verifier).")
    optional_arguments.add_argument("--spake2_verifier", type=base64_str,
                                    help="[base64 string] Provide Spake2+ verifier without generating it.")
    optional_arguments.add_argument("--enable_key", type=str,
                                    help="[hex string] [128-bit hex-encoded] The Enable Key is a 128-bit value that triggers manufacturer-specific action while invoking the TestEventTrigger Command."
                                    "This value is used during Certification Tests, and should not be present on production devices.")
    optional_arguments.add_argument("--user", type=str,
                                    help="[string] Provide additional user-specific keys in JSON format: {'name_1': 'value_1', 'name_2': 'value_2', ... 'name_n', 'value_n'}.")
    optional_arguments.add_argument("--gen_cd", action="store_true", default=False,
                                    help="Generate a new Certificate Declaration in .der format according to used Vendor ID and Product ID. This certificate will not be included to the factory data.")
    optional_arguments.add_argument("--cd_type", type=int, default=1,
                                    help="[int] Type of generated Certification Declaration: 0 - development, 1 - provisional, 2 - official")
    optional_arguments.add_argument("--paa_cert", type=str,
                                    help="Provide a path to the Product Attestation Authority (PAA) certificate to generate the PAI certificate. Without providing it, a testing PAA stored in the Matter repository will be used.")
    optional_arguments.add_argument("--paa_key", type=str,
                                    help="Provide a path to the Product Attestation Authority (PAA) key to generate the PAI certificate. Without providing it, a testing PAA key stored in the Matter repository will be used.")
    args = parser.parse_args()

    if args.verbose:
        log.basicConfig(format='[%(asctime)s][%(levelname)s] %(message)s', level=log.DEBUG)
    else:
        log.basicConfig(format='[%(levelname)s] %(message)s', level=log.INFO)

    # check if json file already exist
    if(exists(args.output) and not args.overwrite):
        log.error("Output file: {} already exist, to create a new one add argument '--overwrite'. By default overwriting is disabled".format(args.output))
        return


    generator = FactoryDataGenerator(args)
    generator.generate_json()


if __name__ == "__main__":
    main()
