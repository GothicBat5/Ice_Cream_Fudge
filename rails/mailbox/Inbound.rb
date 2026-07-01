require "mail"

module ActionMailbox
  
  class InboundEmail < Record
    include Incineratable, MessageId, Routable

    has_one_attached :raw_email, service: ActionMailbox.storage_service
    enum :status, %i[ pending processing delivered failed bounced ]

    def mail
      @mail ||= Mail.from_source(source)
    end

    def source
      @source ||= raw_email.download
    end

    def processed?
      delivered? || failed? || bounced?
    end

    def instrumentation_payload 
      {
        id: id,
        message_id: message_id,
        status: status
      }
    end
  end
end

ActiveSupport.run_load_hooks :action_mailbox_inbound_email, ActionMailbox::InboundEmail
