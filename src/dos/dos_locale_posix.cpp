/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined(WIN32)

#include "dos_locale.h"

#include "checks.h"
#include "dosbox.h"
#include "string_utils.h"

#include <clocale>
#include <string>

CHECK_NARROWING();

static bool is_language_generic(const std::string language)
{
	// XXX - should it go into the table?
	return iequals(language, "C") || iequals(language, "POSIX");
}

// Split locale string into language and territory, drop the rest
static std::pair<std::string, std::string> split_locale(const char *value)
{
	std::string tmp = value; // language[_TERRITORY][.codeset][@modifier]

	tmp = tmp.substr(0, tmp.rfind('@')); // strip the modifier
	tmp = tmp.substr(0, tmp.rfind('.')); // strip the codeset
	
	std::pair<std::string, std::string> result = {};

	result.first = tmp.substr(0, tmp.find('_')); // get the language
	lowcase(result.first);

	const auto position = tmp.rfind('_');
	if (position != std::string::npos) {
		result.second = tmp.substr(position + 1); // get the teritory
		upcase(result.second);
	}

	return result;
}

static const std::map<std::string, DosCountry> PosixToDosCountry = {
	// XXX describe sorting order and search order
	{ "AQ",    DosCountry::International  }, // Antarctica
	{ "EU",    DosCountry::International  }, // European Union
	{ "EZ",    DosCountry::International  }, // Eurozone
	{ "QO",    DosCountry::International  }, // Outlying Oceania
	{ "UN",    DosCountry::International  }, // United Nations
	{ "XX",    DosCountry::International  }, // unknown state
	{ "XZ",    DosCountry::International  }, // international waters
	{ "US",    DosCountry::UnitedStates   },
	{ "JT",    DosCountry::UnitedStates   }, // Johnston Island
	{ "MI",    DosCountry::UnitedStates   }, // Midway Islands
	{ "PU",    DosCountry::UnitedStates   }, // United States Miscellaneous Pacific Islands
	{ "QM",    DosCountry::UnitedStates   }, // used by ISRC
	{ "UM",    DosCountry::UnitedStates   }, // United States Minor Outlying Islands
	{ "VI",    DosCountry::UnitedStates   }, // Virgin Islands (US)
	{ "WK",    DosCountry::UnitedStates   }, // Wake Island
	{ "fr_CA", DosCountry::CanadaFrench   },
	// XXX DosCountry::LatinAmerica
	{ "CA",    DosCountry::CanadaEnglish  },
	{ "RU",    DosCountry::Russia         },
	{ "SU",    DosCountry::Russia         }, // Soviet Union
	{ "EG",    DosCountry::Egypt          },
	{ "ZA",    DosCountry::SouthAfrica    },
	{ "GR",    DosCountry::Greece         },
	{ "NL",    DosCountry::Netherlands    },
	// XXX start checking from here whether we need a separate country for the given territory
	// XXX temporarily add Greenland to Denmark
	{ "AN",    DosCountry::Netherlands    }, // Netherlands Antilles
        { "BQ",    DosCountry::Netherlands    }, // Bonaire, Sint Eustatius and Saba
        { "SX",    DosCountry::Netherlands    }, // Sint Maarten (Dutch part)
	{ "BE",    DosCountry::Belgium        },
	{ "FR",    DosCountry::France         },
	{ "BL",    DosCountry::France         }, // Saint Barthélemy
	{ "CP",    DosCountry::France         }, // Clipperton Island
	{ "FQ",    DosCountry::France         }, // French Southern and Antarctic Territories
	{ "FX",    DosCountry::France         }, // France, Metropolitan
	{ "MF",    DosCountry::France         }, // Saint Martin (French part)
	{ "TF",    DosCountry::France         }, // French Southern Territories
	{ "ES",    DosCountry::Spain          },
	{ "EA",    DosCountry::Spain          }, // Ceuta, Melilla
	{ "IC",    DosCountry::Spain          }, // Canary Islands
	{ "XA",    DosCountry::Spain          }, // Canary Islands, used by Switzerland
	{ "HU",    DosCountry::Hungary        },
	{ "YU",    DosCountry::Yugoslavia     },
	{ "IT",    DosCountry::Italy          },
	{ "VA",    DosCountry::Italy          }, // Vatican City        // XXX we need a separate country
	{ "RO",    DosCountry::Romania        },
	{ "CH",    DosCountry::Switzerland    },
	{ "CZ",    DosCountry::Czechia        },
	{ "CS",    DosCountry::Czechia        }, // Czechoslovakia
	{ "AT",    DosCountry::Austria        },
	{ "GB",    DosCountry::UnitedKingdom  },
	{ "UK",    DosCountry::UnitedKingdom  },
	{ "AC",    DosCountry::UnitedKingdom  }, // Ascension Island 
	{ "CQ",    DosCountry::UnitedKingdom  }, // Island of Sark
	{ "DG",    DosCountry::UnitedKingdom  }, // Diego Garcia
	{ "GG",    DosCountry::UnitedKingdom  }, // Guernsey
	{ "GS",    DosCountry::UnitedKingdom  }, // South Georgia and the South Sandwich Islands
	{ "IM",    DosCountry::UnitedKingdom  }, // Isle of Man
	{ "IO",    DosCountry::UnitedKingdom  }, // British Indian Ocean Territory
	{ "JE",    DosCountry::UnitedKingdom  }, // Jersey
	{ "TA",    DosCountry::UnitedKingdom  }, // Tristan da Cunha
	{ "VG",    DosCountry::UnitedKingdom  }, // Virgin Islands (British)        // XXX we need a separate country
	{ "XI",    DosCountry::UnitedKingdom  }, // Northern Ireland
	{ "DK",    DosCountry::Denmark        },
	{ "SE",    DosCountry::Sweden         },
	{ "NO",    DosCountry::Norway         },
	{ "BV",    DosCountry::Norway         }, // Bouvet Island
	{ "NQ",    DosCountry::Norway         }, // Dronning Maud Land
	{ "SJ",    DosCountry::Norway         }, // Svalbard and Jan Mayen
	{ "PL",    DosCountry::Poland         },
	{ "DE",    DosCountry::Germany        },
	{ "DD",    DosCountry::Germany        }, // German Democratic Republic
	{ "MX",    DosCountry::Mexico         },
	{ "AR",    DosCountry::Argentina      },
	{ "BR",    DosCountry::Brazil         },
	{ "CL",    DosCountry::Chile          },
	{ "CO",    DosCountry::Colombia       },
	{ "VE",    DosCountry::Venezuela      },
	{ "MY",    DosCountry::Malaysia       },
	{ "AU",    DosCountry::Australia      },
	{ "CC",    DosCountry::Australia      }, // Cocos (Keeling) Islands
	{ "CX",    DosCountry::Australia      }, // Christmas Island
	{ "HM",    DosCountry::Australia      }, // Heard Island and McDonald Islands
	{ "NF",    DosCountry::Australia      }, // Norfolk Island
	{ "ID",    DosCountry::Indonesia      },
	{ "PH",    DosCountry::Philippines    },
	{ "NZ",    DosCountry::NewZealand     },
	{ "SG",    DosCountry::Singapore      },
	{ "TH",    DosCountry::Thailand       },
	{ "KZ",    DosCountry::Kazakhstan     },
	{ "JP",    DosCountry::Japan          },
	{ "KR",    DosCountry::SouthKorea     },
	{ "VN",    DosCountry::Vietnam        },
	{ "VD",    DosCountry::Vietnam        }, // North Vietnam
	{ "CN",    DosCountry::China          },
	{ "MO",    DosCountry::China          }, // Macao
	{ "TR",    DosCountry::Turkey         },
	{ "IN",    DosCountry::India          },
	{ "PK",    DosCountry::Pakistan       },
	// XXX DosCountry::AsiaEnglish
	{ "MA",    DosCountry::Morocco        },
	{ "EH",    DosCountry::Morocco        }, // Western Sahara
	{ "DZ",    DosCountry::Algeria        },
	{ "TN",    DosCountry::Tunisia        },
	{ "NE",    DosCountry::Niger          },
	{ "BJ",    DosCountry::Benin          },
	{ "DY",    DosCountry::Benin          }, // Dahomey
	{ "NG",    DosCountry::Nigeria        },
	{ "FO",    DosCountry::FaroeIslands   },
	{ "PT",    DosCountry::Portugal       },
	{ "LU",    DosCountry::Luxembourg     },
	{ "IE",    DosCountry::Ireland        },
	{ "IS",    DosCountry::Iceland        },
	{ "AL",    DosCountry::Albania        },
	{ "MT",    DosCountry::Malta          },
	{ "FI",    DosCountry::Finland        },
	{ "AX",    DosCountry::Finland        }, // Åland Islands
	{ "BG",    DosCountry::Bulgaria       },
	{ "LT",    DosCountry::Lithuania      },
	{ "LV",    DosCountry::Latvia         },
	{ "EE",    DosCountry::Estonia        },
	{ "AM",    DosCountry::Armenia        },
	{ "BY",    DosCountry::Belarus        },
	{ "UA",    DosCountry::Ukraine        },
	{ "RS",    DosCountry::Serbia         },
	{ "ME",    DosCountry::Montenegro     },
	{ "SI",    DosCountry::Slovenia       },
	{ "BA",    DosCountry::BosniaLatin    },
	// TODO: Find a way to detect DosCountry::BosniaCyrillic
	{ "MK",    DosCountry::NorthMacedonia },
	{ "SK",    DosCountry::Slovakia       },
        { "GT",    DosCountry::Guatemala      },
        { "SV",    DosCountry::ElSalvador     },
        { "HN",    DosCountry::Honduras       },
        { "NI",    DosCountry::Nicaragua      },
        { "CR",    DosCountry::CostaRica      },
        { "PA",    DosCountry::Panama         },
        { "PZ",    DosCountry::Panama         }, // Panama Canal Zone
        { "BO",    DosCountry::Bolivia        },
	{ "EC",    DosCountry::Ecuador        },
        { "PY",    DosCountry::Paraguay       },
        { "UY",    DosCountry::Uruguay        },
	// XXX DosCountry::Arabic
        { "NT",    DosCountry::Arabic         }, // Neutral Zone
        { "HK",    DosCountry::HongKong       },
        { "TW",    DosCountry::Taiwan         },
        { "LB",    DosCountry::Lebanon        },
        { "JO",    DosCountry::Jordan         },
        { "SY",    DosCountry::Syria          },
        { "KW",    DosCountry::Kuwait         },
        { "SA",    DosCountry::SaudiArabia    },
        { "YE",    DosCountry::Yemen          },
        { "OM",    DosCountry::Oman           },
        { "AE",    DosCountry::Emirates       },
        { "IL",    DosCountry::Israel         },
        { "BH",    DosCountry::Bahrain        },
        { "QA",    DosCountry::Qatar          },
        { "MN",    DosCountry::Mongolia       },
        { "TJ",    DosCountry::Tajikistan     },
        { "TM",    DosCountry::Turkmenistan   },
        { "AZ",    DosCountry::Azerbaijan     },
        { "GE",    DosCountry::Georgia        },
        { "KG",    DosCountry::Kyrgyzstan     },
        { "UZ",    DosCountry::Uzbekistan     },

	// XXX https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2

        // XXX
        // AD - Andorra
        // CW - Curaçao
        // ER - Eritrea
        // LI - Liechtenstein
        // MC - Monaco
        // PN - Pitcairn
        // PR - Puerto Rico
        // PS - Palestine
        // SM - San Marino
        // SS - South Sudan
        // TL - Timor-Leste

	// XXX misc
	//   XK - Kosovo (temporary designation)
	//   TP - East Timor
	//   CT - Canton and Enderbury Islands
	//   NH - New Hebrides
	//   RH - Southern Rhodesia

	// XXX 150 for English/Europe?
};


static DosCountry get_dos_country(const int category, const DosCountry fallback)
{
	const auto value_ptr = setlocale(category, "");
	if (!value_ptr) {
		return fallback;
	}

	const auto [language, teritory] = split_locale(value_ptr);

	if (is_language_generic(language)) {
		return DosCountry::International;
	}




/* XXX

*/






	return fallback; // XXX
}

HostLocale DOS_DetectHostLocale()
{
	HostLocale locale = {};

	locale.country   = get_dos_country(LC_ALL,      locale.country);
	locale.numeric   = get_dos_country(LC_NUMERIC,  locale.country);
	locale.time_date = get_dos_country(LC_TIME,     locale.country);
	locale.currency  = get_dos_country(LC_MONETARY, locale.country);

	// XXX locale.currency



	// XXX detect the following:
	// XXX locale.messages_language // XXX LC_MESSAGES
	// XXX locale.keyboard_layout

	return locale;
}

#endif
