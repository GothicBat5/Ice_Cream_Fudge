# frozen_string_literal: true
require "active_support"
require "active_support/rails"
require "active_model/version"
require "active_model/deprecator"

module ActiveModel
  extend ActiveSupport::Autoload

  autoload :Access
  autoload :API
  autoload :Attribute
  autoload :Attributes
  autoload :AttributeAssignment
  autoload :AttributeMethods
  autoload :AttributeRegistration
  autoload :BlockValidator, "active_model/validator"
  autoload :Callbacks
  autoload :Conversion
  autoload :Dirty
  autoload :EachValidator, "active_model/validator"
  autoload :ForbiddenAttributesProtection
  autoload :Lint
  autoload :Model
  autoload :Name, "active_model/naming"
  autoload :Naming
  autoload :SchematizedJson
  autoload :SecurePassword
  autoload :Serialization
  autoload :Translation
  autoload :Type
  autoload :Validations
  autoload :Validator

  eager_autoload do
    autoload :Errors
    autoload :Error
    autoload :RangeError, "active_model/errors"
    autoload :StrictValidationFailed, "active_model/errors"
    autoload :UnknownAttributeError, "active_model/errors"
    autoload :ValidationError, "active_model/validations"
  end

  module Serializers
    extend ActiveSupport::Autoload

    eager_autoload do
      autoload :JSON
    end
  end

  def self.eager_load!
    super
    ActiveModel::Serializers.eager_load!
  end
end

ActiveSupport.on_load(:i18n) do
  I18n.load_path << File.expand_path("active_model/locale/en.yml", __dir__)
end
